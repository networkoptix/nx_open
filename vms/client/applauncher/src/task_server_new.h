#pragma once

#include <memory>

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/ipc/named_pipe_server.h>

namespace nx::vms::applauncher {

/**
 * Accepts tasks from application instances and stores them in a task queue.
 *
 * Implemented using own implementation of NamedPipeServer, since Qt's QLocalServer creates win32
 * pipe only for current user
 */
class TaskServerNew: public QnLongRunnable
{
public:
    TaskServerNew();
    virtual ~TaskServerNew() override;

    virtual void pleaseStop() override;

    /**
     * Only one server can listen to the pipe (despite QLocalServer).
     * @returns false if failed (e.g. someone uses it) to bind to local address pipeName.
     */
    bool listen(const QString& pipeName);

    enum class ResponseError
    {
        noError,
        noChannel,
        failedToDeserialize,
    };

    /** Channel for a single comamnd type. Incoming packets are dispatched to this channels. */
    struct Channel
    {
        QByteArray name;
        using Callback = std::function<ResponseError (Channel&, const QByteArray&, QByteArray&)>;
        Callback requestParser;
    };

    /**
     * Add a subscriber to a specified channel.
     * Will return false if specified channel is already occupied
     * or empty name or callback is provided.
     */
    template<class RequestType, class ResponseType>
    bool subscribe(
        const QByteArray& name,
        std::function<void (const RequestType&, ResponseType&)>&& callback)
    {
        return subscribe(name,
            [callback](
                Channel& /*channel*/,
                const QByteArray& requestRawData,
                QByteArray& responseRawData)
            {
                RequestType request;
                ResponseType response;
                if (!request.deserialize(requestRawData))
                    return ResponseError::failedToDeserialize;
                callback(request, response);
                responseRawData = response.serialize();
                return ResponseError::noError;
            });
    }

    /** Adds a simple 'responseless' subscriber. */
    bool subscribe(const QByteArray& name, std::function<bool (const QByteArray& name)> callback);

protected:
    virtual void run() override;

private:
    void processNewConnection(NamedPipeSocket* clientConnection);
    bool subscribe(const QByteArray& name, Channel::Callback&& callback);

private:
    NamedPipeServer m_server;
    QString m_pipeName;
    bool m_terminated = false;
    QHash<QByteArray, Channel> m_channels;

};

} // namespace nx::vms::applauncher
