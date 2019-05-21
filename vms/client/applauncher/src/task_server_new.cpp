#include "task_server_new.h"

#if !defined(WIN32)
#include <unistd.h>
#endif

#include <qtsinglecoreapplication.h>

#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>

#include "abstract_request_processor.h"

namespace nx::applauncher {

TaskServerNew::TaskServerNew(AbstractRequestProcessor* const requestProcessor):
    m_requestProcessor(requestProcessor)
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

    if (!nx::utils::AppInfo::isWindows())
    {
        // Removing unix socket file in case it hanged after a process crash.
        const QByteArray filePath = ("/tmp/" + pipeName).toLatin1();
        unlink(filePath.constData());
    }

    const SystemError::ErrorCode osError = m_server.listen(pipeName);
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

    while (!m_terminated)
    {
        NamedPipeSocket* clientConnection = nullptr;
        const auto osError = m_server.accept(&clientConnection, kClientConnectionTimeoutMs);

        std::unique_ptr<NamedPipeSocket> clientConnectionPtr(clientConnection);

        if (osError != SystemError::noError)
        {
            if (osError == SystemError::timedOut)
                continue;

            NX_DEBUG(this, "Failed to listen to pipe %1. %2", m_pipeName, osError);

            msleep(kErrorSleepTimeoutMs);
            continue;
        }

        processNewConnection(clientConnectionPtr.get());
    }
}

void TaskServerNew::processNewConnection(NamedPipeSocket* clientConnection)
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

    api::BaseTask* task = nullptr;
    if (!deserializeTask(QByteArray::fromRawData(buffer, (int) bytesRead), &task))
    {
        NX_DEBUG(this, "Failed to deserialize received task data");
        return;
    }

    if (task->type == api::TaskType::quit)
        m_terminated = true;

    api::Response* response = nullptr;
    m_requestProcessor->processRequest(std::shared_ptr<api::BaseTask>(task), &response);

    const QByteArray& responseMsg = (response == nullptr) ? "ok\n\n" : response->serialize();
    unsigned int bytesWritten = 0;
    clientConnection->write(responseMsg.constData(), (unsigned) responseMsg.size(), &bytesWritten);
    clientConnection->flush();
}

} // namespace nx::applauncher
