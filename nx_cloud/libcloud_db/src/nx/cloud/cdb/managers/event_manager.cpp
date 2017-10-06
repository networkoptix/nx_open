/**********************************************************
* May 11, 2016
* a.kolesnikov
***********************************************************/

#include "event_manager.h"

#include <chrono>

#include <nx/network/http/multipart_msg_body_source.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/cdb/client/cdb_request_path.h>

#include "../http_handlers/base_http_handler.h"
#include "../settings.h"

namespace nx {
namespace cdb {

class SubscribeToSystemEventsHandler:
    public AbstractFreeMsgBodyHttpHandler<>
{
public:
    static const QString kHandlerPath;

    SubscribeToSystemEventsHandler(
        EventManager* const eventManager,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFreeMsgBodyHttpHandler<>(
            EntityType::system,
            DataActionType::fetch,
            authorizationManager,
            [eventManager](
                nx_http::HttpServerConnection* connection,
                const AuthorizationInfo& authzInfo,
                nx::utils::MoveOnlyFunc<
                    void(api::ResultCode, std::unique_ptr<nx_http::AbstractMsgBodySource>)
                > completionHandler)
            {
                eventManager->subscribeToEvents(
                    connection,
                    authzInfo,
                    std::move(completionHandler));
            })
    {
    }
};

const QString SubscribeToSystemEventsHandler::kHandlerPath = QLatin1String(kSubscribeToSystemEventsPath);


EventManager::EventManager(const conf::Settings& settings)
:
    m_settings(settings)
{
}

EventManager::~EventManager()
{
    //assuming no public methods of this class can be called anymore,
    //  but we have to wait for all scheduled async calls to finish
    m_startedAsyncCallsCounter.wait();
}

void EventManager::registerHttpHandlers(
    const AuthorizationManager& authorizationManager,
    nx_http::server::rest::MessageDispatcher* const httpMessageDispatcher)
{
    httpMessageDispatcher->registerRequestProcessor<SubscribeToSystemEventsHandler>(
        SubscribeToSystemEventsHandler::kHandlerPath,
        [this, &authorizationManager]() -> std::unique_ptr<SubscribeToSystemEventsHandler>
        {
            return std::make_unique<SubscribeToSystemEventsHandler>(
                this,
                authorizationManager);
        });
}

void EventManager::subscribeToEvents(
    nx_http::HttpServerConnection* httpConnection,
    const AuthorizationInfo& authzInfo,
    nx::utils::MoveOnlyFunc<
        void(api::ResultCode, std::unique_ptr<nx_http::AbstractMsgBodySource>)
    > completionHandler)
{
    std::string systemId;
    if (!authzInfo.get(cdb::attr::authSystemId, &systemId))
    {
        NX_ASSERT(false);
        completionHandler(
            api::ResultCode::unknownError,
            nullptr);
        return;
    }

    NX_LOGX(lm("System %1 subscribing to events").arg(systemId), cl_logDEBUG2);

    auto msgBody =
        std::make_unique<nx_http::MultipartMessageBodySource>(
            "nx_cloud_event_boundary");

    {
        QnMutexLocker lk(&m_mutex);

        //checking if such connection is already present
        const auto iterAndInsertionFlag = m_activeMediaServerConnections.emplace(
            ServerConnectionContext(httpConnection, systemId));

        msgBody->setOnBeforeDestructionHandler(
            std::bind(&EventManager::beforeMsgBodySourceDestruction, this, httpConnection));
        if (iterAndInsertionFlag.second)
            httpConnection->registerCloseHandler(
                std::bind(&EventManager::onConnectionToPeerLost, this,
                    iterAndInsertionFlag.first));

        m_activeMediaServerConnections.modify(
            iterAndInsertionFlag.first,
            [&msgBody, &iterAndInsertionFlag, &httpConnection, this](
                ServerConnectionContext& context)
            {
                if (!context.timer)
                {
                    context.timer = std::make_unique<nx::network::aio::Timer>();
                    context.timer->bindToAioThread(httpConnection->getAioThread());
                }
                context.msgBody = msgBody.get();
                context.timer->start(
                    m_settings.eventManager().mediaServerConnectionIdlePeriod,
                    std::bind(&EventManager::onMediaServerIdlePeriodExpired, this, iterAndInsertionFlag.first));
            });
    }

    completionHandler(
        api::ResultCode::ok,
        std::move(msgBody));
}

bool EventManager::isSystemOnline(const std::string& systemId) const
{
    QnMutexLocker lk(&m_mutex);

    const auto& connectionBySystemIdIndex = 
        m_activeMediaServerConnections.get<kServerContextBySystemIdIndex>();
    return connectionBySystemIdIndex.find(systemId) !=
        connectionBySystemIdIndex.end();
}

void EventManager::beforeMsgBodySourceDestruction(
    nx_http::HttpServerConnection* connection)
{
    //element can already be removed, but connection object is still alive

    QnMutexLocker lk(&m_mutex);

    const auto& serverConnectionIndex = 
        m_activeMediaServerConnections.get<kServerContextByConnectionIndex>();
    auto serverConnectionIter = serverConnectionIndex.find(connection);
    if (serverConnectionIter == serverConnectionIndex.end())
        return;

    m_activeMediaServerConnections.modify(
        serverConnectionIter,
        [](ServerConnectionContext& context)
        {
            context.msgBody = nullptr;
            context.timer->cancelSync();
        });
}

void EventManager::onConnectionToPeerLost(
    EventManager::MediaServerConnectionContainer::iterator serverConnectionIter)
{
    QnMutexLocker lk(&m_mutex);
    m_activeMediaServerConnections.erase(serverConnectionIter);
}

void EventManager::onMediaServerIdlePeriodExpired(
    EventManager::MediaServerConnectionContainer::iterator serverConnectionIter)
{
    //closing event stream to force mediaserver send us a new request

    //NOTE we do not need synchronization here since all events 
    //    related to a single connection are always invoked within same aio thread
    m_activeMediaServerConnections.modify(
        serverConnectionIter,
        [](ServerConnectionContext& context)
        {
            context.msgBody->serializer()->writeEpilogue();
        });
}

}   //namespace cdb
}   //namespace nx
