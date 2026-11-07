// SPDX-License-Identifier: GPL-3.0-only
#include "ServerProcess.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace {
constexpr int kStopTimeoutMs = 30000;
constexpr int kMaxLogLines = 5000;
}  // namespace

ServerProcess::ServerProcess(const ServerConfig& config, const QString& directory, QObject* parent)
    : QObject(parent), m_config(config), m_directory(directory)
{
    m_stopTimer.setSingleShot(true);
    m_stopTimer.setInterval(kStopTimeoutMs);
    connect(&m_stopTimer, &QTimer::timeout, this, &ServerProcess::onStopTimeout);
}

ServerProcess::~ServerProcess()
{
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
    }
}

QString ServerProcess::resolveJava() const
{
    if (!m_config.javaPath.isEmpty() && QFile::exists(m_config.javaPath)) {
        return m_config.javaPath;
    }
    // TODO(stage 1b): pick the right Java through the launcher's Java list using
    // ServerConfig::guessJavaMajor(); until then fall back to whatever is on PATH.
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

    m_log.clear();
    if (m_process) {
        m_process->disconnect(this);
        m_process->deleteLater();
    }
    m_process = new LoggedProcess(QStringConverter::Utf8, this);
    connect(m_process, &LoggedProcess::log, this, &ServerProcess::onLog);
    connect(m_process, &LoggedProcess::stateChanged, this, &ServerProcess::onProcessState);
    m_process->setWorkingDirectory(m_directory);
    m_process->setProgram(java);
    m_process->setArguments(buildArguments());
    setStatus(Status::Starting);
    m_process->start();
    return true;
}

void ServerProcess::stop()
{
    if (!m_process || !isActive() || m_status == Status::Stopping) {
        return;
    }
    setStatus(Status::Stopping);
    m_process->write("stop\n");
    m_stopTimer.start();
}

void ServerProcess::kill()
{
    m_stopTimer.stop();
    if (m_process) {
        m_process->kill();
    }
}

void ServerProcess::sendCommand(const QString& command)
{
    if (m_process && (m_status == Status::Running || m_status == Status::Starting)) {
        m_process->write((command + QLatin1Char('\n')).toUtf8());
    }
}

void ServerProcess::onLog(QStringList lines, MessageLevel /*level*/)
{
    m_log.append(lines);
    if (m_log.size() > kMaxLogLines) {
        m_log = m_log.mid(m_log.size() - kMaxLogLines);
    }
    // Vanilla/Paper/Fabric all print 'Done (x.xs)! For help, type "help"' when ready.
    if (m_status == Status::Starting) {
        for (const QString& line : lines) {
            if (line.contains(QStringLiteral("Done (")) && line.contains(QStringLiteral("For help"))) {
                setStatus(Status::Running);
                break;
            }
        }
    }
    emit logAppended(lines);
}

void ServerProcess::onProcessState(LoggedProcess::State state)
{
    switch (state) {
        case LoggedProcess::Finished:
        case LoggedProcess::Aborted:
            m_stopTimer.stop();
            setStatus(Status::Stopped);
            break;
        case LoggedProcess::Crashed:
        case LoggedProcess::FailedToStart:
            m_stopTimer.stop();
            setStatus(m_status == Status::Stopping ? Status::Stopped : Status::Crashed);
            break;
        default:
            break;
    }
}

void ServerProcess::onStopTimeout()
{
    if (m_status == Status::Stopping && m_process) {
        m_process->kill();
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
