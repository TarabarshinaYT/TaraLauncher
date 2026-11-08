// SPDX-License-Identifier: GPL-3.0-only
#include "ServerProcess.h"

#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include "PortAllocator.h"
#include "RconClient.h"
#include "ServerProperties.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <csignal>
#include <cerrno>
#endif

namespace {
constexpr int kPollMs = 700;
constexpr int kStopTimeoutMs = 30000;
constexpr int kMaxLogLines = 5000;
constexpr qint64 kTailBytes = 64 * 1024;

bool pidAlive(qint64 pid)
{
    if (pid <= 0) {
        return false;
    }
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h) {
        return false;
    }
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
#else
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

void killPid(qint64 pid)
{
    if (pid <= 0) {
        return;
    }
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (h) {
        TerminateProcess(h, 1);
        CloseHandle(h);
    }
#else
    ::kill(static_cast<pid_t>(pid), SIGKILL);
#endif
}
}  // namespace

ServerProcess::ServerProcess(const ServerConfig& config, const QString& directory, QObject* parent)
    : QObject(parent), m_config(config), m_directory(directory), m_history(directory)
{
    m_poll.setInterval(kPollMs);
    connect(&m_poll, &QTimer::timeout, this, &ServerProcess::poll);

    // The launcher may have been restarted while the server kept running: pick it up again.
    QFile pidFile(pidFilePath());
    if (pidFile.open(QIODevice::ReadOnly)) {
        const qint64 pid = pidFile.readAll().trimmed().toLongLong();
        pidFile.close();
        if (pidAlive(pid)) {
            m_pid = pid;
            m_status = Status::Running;
            // show the end of the existing log
            QFile log(logFilePath());
            if (log.open(QIODevice::ReadOnly)) {
                const qint64 size = log.size();
                log.seek(qMax<qint64>(0, size - kTailBytes));
                m_log = QString::fromUtf8(log.readAll()).split('\n', Qt::SkipEmptyParts);
                m_logOffset = size;
            }
            m_needsReconcile = true;  // who is online right now? asked through RCON on the first poll
            m_poll.start();
        } else {
            QFile::remove(pidFilePath());
        }
    }
    if (m_status == Status::Stopped) {
        // sessions left open mean the server ended while the launcher was closed (or the PC was switched off)
        m_history.closeAll(QDateTime::currentDateTime());
    }
}

QString ServerProcess::pidFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("tara-server.pid"));
}

QString ServerProcess::logFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("logs/latest.log"));
}

void ServerProcess::writePidFile() const
{
    QFile f(pidFilePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QByteArray::number(m_pid));
    }
}

QString ServerProcess::resolveJava() const
{
    if (!m_config.javaPath.isEmpty() && QFile::exists(m_config.javaPath)) {
        return m_config.javaPath;
    }
    // TODO: pick/download the right Java via the launcher's Java list (ServerConfig::guessJavaMajor()).
    return QStandardPaths::findExecutable(QStringLiteral("java"));
}

QStringList ServerProcess::buildArguments() const
{
    QStringList args;
    args << QStringLiteral("-Xms%1M").arg(qMin(m_config.memoryMb, 512));
    args << QStringLiteral("-Xmx%1M").arg(m_config.memoryMb);
    args << m_config.extraJvmArgs;
    if (m_config.launchArgs.isEmpty()) {
        args << "-jar" << "server.jar" << "nogui";
    } else {
        args << m_config.launchArgs;
    }
    return args;
}

bool ServerProcess::ensureRcon(QString* error)
{
    bool changed = false;
    if (m_config.rconPassword.isEmpty()) {
        m_config.rconPassword = Rcon::makePassword();
        changed = true;
    }
    if (m_config.rconPort <= 0 || m_config.rconPort == m_config.port || !ServerPorts::isFree(m_config.rconPort)) {
        const int port = ServerPorts::findFree({ m_config.port }, 25575);
        if (port < 0) {
            if (error) {
                *error = tr("Не нашлось свободного порта для RCON.");
            }
            return false;
        }
        m_config.rconPort = port;
        changed = true;
    }

    ServerProperties props;
    const QString path = QDir(m_directory).filePath(QStringLiteral("server.properties"));
    props.load(path);
    props.setInt(QStringLiteral("server-port"), m_config.port);
    props.setBool(QStringLiteral("enable-rcon"), true);
    props.setInt(QStringLiteral("rcon.port"), m_config.rconPort);
    props.set(QStringLiteral("rcon.password"), m_config.rconPassword);
    if (!props.save(path)) {
        if (error) {
            *error = tr("Не удалось записать server.properties.");
        }
        return false;
    }
    if (changed) {
        m_config.save(m_directory);
    }
    return true;
}

bool ServerProcess::start()
{
    if (isActive()) {
        return false;
    }
    if (!m_config.eulaAccepted) {
        emit failed(tr("Нужно принять EULA Mojang, иначе сервер не запустится."));
        return false;
    }
    const QString java = resolveJava();
    if (java.isEmpty()) {
        emit failed(tr("Java не найдена. Укажите путь к Java в настройках сервера."));
        return false;
    }
    if (!ServerPorts::isFree(m_config.port)) {
        emit failed(tr("Порт %1 уже занят. Остановите другую программу или смените порт сервера.").arg(m_config.port));
        return false;
    }
    QString error;
    if (!ensureRcon(&error)) {
        emit failed(error);
        return false;
    }

    // remember where the old log ends, the server rotates latest.log when it starts
    m_log.clear();
    QFile oldLog(logFilePath());
    m_logOffset = oldLog.exists() ? oldLog.size() : 0;

    QProcess process;
    process.setProgram(java);
    process.setArguments(buildArguments());
    process.setWorkingDirectory(m_directory);
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
        args->flags &= ~static_cast<DWORD>(CREATE_NEW_CONSOLE);
    });
#endif
    qint64 pid = 0;
    if (!process.startDetached(&pid) || pid <= 0) {
        emit failed(tr("Не удалось запустить Java: %1").arg(java));
        return false;
    }
    m_pid = pid;
    m_stopRequested = false;
    writePidFile();
    setStatus(Status::Starting);
    m_poll.start();
    return true;
}

void ServerProcess::stop()
{
    if (!isActive() || m_status == Status::Stopping) {
        return;
    }
    m_stopRequested = true;
    m_stopClock.start();
    setStatus(Status::Stopping);
    QString error;
    if (!Rcon::send(m_config.rconPort, m_config.rconPassword, QStringLiteral("stop"), nullptr, &error)) {
        // the server may still be loading; poll() force-kills after the timeout
        appendLines({ tr("[TaraLauncher] Не удалось отправить stop: %1").arg(error) });
    }
}

void ServerProcess::kill()
{
    m_stopRequested = true;
    killPid(m_pid);
}

void ServerProcess::sendCommand(const QString& command)
{
    if (m_status != Status::Running && m_status != Status::Starting) {
        return;
    }
    QString reply, error;
    appendLines({ QStringLiteral("> ") + command });
    if (!Rcon::send(m_config.rconPort, m_config.rconPassword, command, &reply, &error)) {
        appendLines({ tr("[TaraLauncher] %1").arg(error) });
        return;
    }
    if (!reply.trimmed().isEmpty()) {
        appendLines(reply.split('\n', Qt::SkipEmptyParts));
    }
}

bool ServerProcess::query(const QString& command, QString* reply)
{
    if (m_status != Status::Running) {
        return false;
    }
    return Rcon::send(m_config.rconPort, m_config.rconPassword, command, reply, nullptr);
}

void ServerProcess::appendLines(const QStringList& lines)
{
    if (lines.isEmpty()) {
        return;
    }
    m_log.append(lines);
    if (m_log.size() > kMaxLogLines) {
        m_log = m_log.mid(m_log.size() - kMaxLogLines);
    }
    emit logAppended(lines);
}

void ServerProcess::readNewLog()
{
    QFile log(logFilePath());
    if (!log.open(QIODevice::ReadOnly)) {
        return;
    }
    if (log.size() < m_logOffset) {
        m_logOffset = 0;  // the server rotated the file
    }
    log.seek(m_logOffset);
    const QByteArray data = log.readAll();
    m_logOffset += data.size();
    if (data.isEmpty()) {
        return;
    }
    const QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);
    if (m_status == Status::Starting) {
        for (const QString& line : lines) {
            if (line.contains(QStringLiteral("Done (")) && line.contains(QStringLiteral("For help"))) {
                setStatus(Status::Running);
                break;
            }
        }
    }
    appendLines(lines);
    parseEvents(lines);
}

void ServerProcess::parseEvents(const QStringList& lines)
{
    // "[12:00:00] [Server thread/INFO]: Steve joined the game" (vanilla, Fabric) and
    // "[12:00:00 INFO]: Steve joined the game" (Paper). The name must directly follow "]: ",
    // so chat messages like "<Steve> joined the game" do not match.
    static const QRegularExpression presence(QStringLiteral("\\]: ([A-Za-z0-9_.]{1,32}) (joined|left) the game\\s*$"));
    static const QRegularExpression errorLine(QStringLiteral("(/ERROR\\]|\\sERROR\\]):"));
    const QDateTime now = QDateTime::currentDateTime();
    for (const QString& line : lines) {
        const auto m = presence.match(line);
        if (m.hasMatch()) {
            const QString name = m.captured(1);
            if (m.captured(2) == QLatin1String("joined")) {
                m_history.playerJoined(name, now);
                emit playerJoined(name);
            } else {
                m_history.playerLeft(name, now);
                emit playerLeft(name);
            }
            continue;
        }
        if (errorLine.match(line).hasMatch()) {
            // at most one error notification per minute, a crash can print hundreds of lines
            if (!m_errorClock.isValid() || m_errorClock.elapsed() > 60000) {
                m_errorClock.start();
                emit errorLogged(line);
            }
        }
    }
}

void ServerProcess::reconcileOnlinePlayers()
{
    QString reply, error;
    if (!Rcon::send(m_config.rconPort, m_config.rconPassword, QStringLiteral("list"), &reply, &error)) {
        return;
    }
    QStringList online;
    for (const QString& name : m_history.onlineNames()) {
        const QRegularExpression word(QStringLiteral("(^|[^A-Za-z0-9_.])%1($|[^A-Za-z0-9_.])").arg(QRegularExpression::escape(name)));
        if (word.match(reply).hasMatch()) {
            online << name;
        }
    }
    m_history.keepOnly(online, QDateTime::currentDateTime());
}

void ServerProcess::poll()
{
    if (m_needsReconcile && m_status == Status::Running) {
        m_needsReconcile = false;
        reconcileOnlinePlayers();
    }
    readNewLog();

    if (!pidAlive(m_pid)) {
        readNewLog();
        m_poll.stop();
        QFile::remove(pidFilePath());
        m_pid = 0;
        const bool expected = m_stopRequested || m_status == Status::Stopping;
        m_history.closeAll(QDateTime::currentDateTime());
        setStatus(expected ? Status::Stopped : Status::Crashed);
        if (!expected) {
            emit crashed();
        }
        return;
    }
    if (m_status == Status::Stopping && m_stopClock.isValid() && m_stopClock.elapsed() > kStopTimeoutMs) {
        appendLines({ tr("[TaraLauncher] Сервер не остановился за 30 секунд, завершаю принудительно.") });
        killPid(m_pid);
        m_stopClock.invalidate();
    }
}

void ServerProcess::setStatus(Status status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged(status);
}
