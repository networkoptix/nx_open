// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "task_server_new.h"

#if !defined(WIN32)
#include <unistd.h>
#endif

#include <qtsinglecoreapplication.h>

#include <nx/utils/log/log.h>

#include <nx/build_info.h>

namespace nx::vms::applauncher {

TaskServerNew::TaskServerNew()
{
}

TaskServerNew::~TaskServerNew()
{
    stop();
}

void TaskServerNew::pleaseStop()
{
    m_terminated = true;
}

bool TaskServerNew::listen(const QString& pipeName)
{
    if (isRunning())
    {
        pleaseStop();
        wait();
    }

    // On Windows multiple m_taskServer can listen a single pipe, on linux unix socket can hang
    // after server crash (because of socket file not removed).
    const auto app = qobject_cast<QtSingleCoreApplication*>(QCoreApplication::instance());
    if (!NX_ASSERT(app))
        return false;

    if (app->isRunning())
    {
        NX_DEBUG(this, "Application instance is already running. Not listening to pipe.");
        return false;
    }

    if (!nx::build_info::isWindows())
    {
        // Removing unix socket file in case it hanged after a process crash.
        const QByteArray filePath = ("/tmp/" + pipeName).toLatin1();
        unlink(filePath.constData());
    }

    const SystemError::ErrorCode osError = m_server.listen(pipeName.toStdString());
    if (osError != SystemError::noError)
    {
        NX_DEBUG(this, "Failed to listen to pipe %1. %2",
            pipeName, SystemError::toString(osError));
        return false;
    }

    m_pipeName = pipeName;

    NX_DEBUG(this, "Listening pipe %1", pipeName);

    start();

    return true;
}

void TaskServerNew::run()
{
    static const int kErrorSleepTimeoutMs = 1000;
    static const int kClientConnectionTimeoutMs = 1000;
    static const int kMaxAcceptErrorCount = 5;

    int acceptErrorCount = 0;

    while (!m_terminated)
    {
        nx::utils::NamedPipeSocket* clientConnection = nullptr;
        const auto osError = m_server.accept(&clientConnection, kClientConnectionTimeoutMs);

        std::unique_ptr<nx::utils::NamedPipeSocket> clientConnectionPtr(clientConnection);

        if (osError != SystemError::noError)
        {
            if (osError == SystemError::timedOut)
                continue;

            NX_DEBUG(this, "Failed to listen to pipe %1. %2", m_pipeName, osError);

            if (++acceptErrorCount >= kMaxAcceptErrorCount)
            {
                NX_DEBUG(this, "Too many errors while accepting connections. Stopping the server.");
                break;
            }

            msleep(kErrorSleepTimeoutMs);
            continue;
        }

        acceptErrorCount = 0;

        processNewConnection(clientConnectionPtr.get());
    }
}

void TaskServerNew::processNewConnection(nx::utils::NamedPipeSocket* clientConnection)
{
    static const int kMaxMessageSize = 4 * 1024;

    char buffer[kMaxMessageSize];

    unsigned int bytesRead = 0;
    SystemError::ErrorCode result = clientConnection->read(buffer, kMaxMessageSize, &bytesRead);
    if (result != SystemError::noError)
    {
        NX_DEBUG(this, "Failed to receive task data. %1", result);
        return;
    }

    auto requestRawData = QByteArray::fromRawData(buffer, (int) bytesRead);
    const int taskNameEnd = requestRawData.indexOf('\n');
    if (taskNameEnd == -1)
    {
        NX_ERROR(NX_SCOPE_TAG, "deserializeTask(): Cannot deserialize task name:\n%1",
            QString::fromUtf8(requestRawData));
        return;
    }

    const QByteArray taskName(requestRawData.constData(), taskNameEnd);
    if (m_channels.contains(taskName))
    {
        auto& channel = m_channels[taskName];
        QByteArray responseRawData;
        auto code = channel.requestParser(channel, requestRawData, responseRawData);
        if (code == ResponseError::noError)
        {
            if (responseRawData.isEmpty())
                responseRawData = "ok\n\n";
            unsigned int bytesWritten = 0;
            clientConnection->write(
                responseRawData.constData(),
                (unsigned) responseRawData.size(),
                &bytesWritten);
        }
        else if (code == ResponseError::failedToDeserialize)
        {
            NX_ERROR(this, "Failed to deserialize request \"%1\"", taskName);
            return;
        }
        else
        {
            NX_ERROR(this, "Unknown error while processing request \"%1\"", taskName);
            return;
        }
    }
    else
    {
        NX_ERROR(this, "Unknown channel \"%1\"", taskName);
    }
    clientConnection->flush();
}

/** Adds a simple 'responseless' subscriber. */
bool TaskServerNew::subscribeSimple(const QByteArray& name, std::function<bool ()> callback)
{
    return subscribeImpl(name,
        [callback](
            Channel& /*channel*/,
            const QByteArray& /*requestRawData*/,
            QByteArray& /*responseRawData*/)
        {
            if (callback())
                return ResponseError::noError;
            return ResponseError::failedToDeserialize;
        });
}

bool TaskServerNew::subscribeImpl(const QByteArray& name, Channel::Callback&& callback)
{
    NX_ASSERT(!name.isEmpty());
    NX_ASSERT(callback);
    if (name.isEmpty() || !callback)
        return false;
    if (m_channels.contains(name))
        return false;

    m_channels.insert(name, {name, callback});
    return true;
}

} // namespace nx::vms::applauncher
