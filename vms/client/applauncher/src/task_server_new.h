#pragma once

#include <memory>

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/ipc/named_pipe_server.h>

namespace applauncher {

class AbstractRequestProcessor;

/**
 * Accepts tasks from application instances and stores them in a task queue.
 *
 * Implemented using own implementation of NamedPipeServer, since Qt's QLocalServer creates win32
 * pipe only for current user
 */
class TaskServerNew: public QnLongRunnable
{
public:
    TaskServerNew(AbstractRequestProcessor* const requestProcessor);
    virtual ~TaskServerNew() override;

    virtual void pleaseStop() override;

    /**
     * Only one server can listen to the pipe (despite QLocalServer).
     * @returns false if failed (e.g. someone uses it) to bind to local address pipeName.
     */
    bool listen(const QString& pipeName);

protected:
    virtual void run() override;

private:
    void processNewConnection(NamedPipeSocket* clientConnection);

private:
    AbstractRequestProcessor* const m_requestProcessor;
    NamedPipeServer m_server;
    QString m_pipeName;
    bool m_terminated = false;
};

} // namespace applauncher
