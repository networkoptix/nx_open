#pragma once

#include <memory>

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/ipc/named_pipe_server.h>
#include <nx/utils/signature_extractor.h>
#include <nx/vms/applauncher/api/applauncher_api.h>

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
     * Example usage:
     * ```
     * auto callback = [](const SomeRequest& req, SomeResponse& rep)
     *     {
     *         ...
     *     };
     * taskServer.subscribe("request1", callback);
     * ```
     */
    template<class CallbackType>
    bool subscribe(const QByteArray& name, CallbackType&& callback)
    {
        using RequestType = typename nx::utils::FunctionArgumentType<CallbackType, 0>::RawType;
        static_assert(std::is_base_of<nx::vms::applauncher::api::BaseTask, RequestType>::value,
            "First argument should be a subclass of a BaseTask class");
        using ResponseType = typename nx::utils::FunctionArgumentType<CallbackType, 1>::RawType;
        static_assert(std::is_base_of<nx::vms::applauncher::api::Response, ResponseType>::value,
            "Second argument should be a subclass of a Response class");
        /*
         * TODO: Though there are static_assert for type checking, there will be lots of other
         * compile errors in a lambda below. I could add some magic to prevent code below from
         * compiling at all, so user will see only errors from static assert.
         */
        return subscribeImpl(name,
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
    bool subscribeSimple(const QByteArray& name, std::function<bool ()> callback);

protected:
    virtual void run() override;

private:
    void processNewConnection(NamedPipeSocket* clientConnection);
    bool subscribeImpl(const QByteArray& name, Channel::Callback&& callback);

private:
    NamedPipeServer m_server;
    QString m_pipeName;
    bool m_terminated = false;
    QHash<QByteArray, Channel> m_channels;

};

} // namespace nx::vms::applauncher
