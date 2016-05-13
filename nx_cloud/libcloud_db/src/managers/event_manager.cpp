/**********************************************************
* May 11, 2016
* a.kolesnikov
***********************************************************/

#include "event_manager.h"

#include <chrono>

#include <nx/network/http/multipart_msg_body_source.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/utils/log/log.h>
#include <utils/common/cpp14.h>

#include <cloud_db_client/src/cdb_request_path.h>

#include "http_handlers/base_http_handler.h"
#include "settings.h"


namespace nx {
namespace cdb {

class SubscribeToSystemEventsHandler
:
    public AbstractFreeMsgBodyHttpHandler<void>
{
public:
    static const QString kHandlerPath;

    SubscribeToSystemEventsHandler(
        EventManager* const eventManager,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFreeMsgBodyHttpHandler<void>(
            EntityType::system,
            DataActionType::fetch,
            authorizationManager,
            std::bind(
                &EventManager::subscribeToEvents, eventManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
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
    nx_http::MessageDispatcher* const httpMessageDispatcher)
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
    const nx_http::HttpServerConnection& httpConnection,
    const AuthorizationInfo& authzInfo,
    nx::utils::MoveOnlyFunc<
        void(api::ResultCode, std::unique_ptr<nx_http::AbstractMsgBodySource>)
    > completionHandler)
{
    NX_LOGX(lm("subscribeToEvents"), cl_logDEBUG2);

    auto systemID = authzInfo.get<std::string>(cdb::attr::authSystemID);
    if (!systemID)
    {
        NX_ASSERT(false);
        completionHandler(
            api::ResultCode::unknownError,
            nullptr);
        return;
    }

    NX_LOGX(lm("System %1 subscribing to events").arg(*systemID), cl_logDEBUG2);

    auto context = std::make_unique<ServerConnectionContext>();
    auto msgBody = 
        std::make_unique<nx_http::MultipartMessageBodySource>(
            "nx_cloud_event_boundary");
    context->msgBody = msgBody.get();

    {
        QnMutexLocker lk(&m_mutex);
        auto it = m_activeMediaServerConnections.emplace(*systemID, std::move(context));
        it->second->msgBody->setOnBeforeDestructionHandler(
            std::bind(&EventManager::beforeMsgBodySourceDestruction, this, it));
        it->second->timer.bindToAioThread(httpConnection.getAioThread());
        it->second->timer.start(
            m_settings.eventManager().mediaServerConnectionIdlePeriod,
            std::bind(&EventManager::onMediaServerIdlePeriodExpired, this, it));
    }

    completionHandler(
        api::ResultCode::ok,
        std::move(msgBody));
}

bool EventManager::isSystemOnline(const std::string& systemId) const
{
    QnMutexLocker lk(&m_mutex);
    return m_activeMediaServerConnections.find(systemId) != 
        m_activeMediaServerConnections.end();
}

void EventManager::beforeMsgBodySourceDestruction(
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
        //related to a single connection are always invoked within same aio thread
    serverConnectionIter->second->msgBody->serializer()->writeEpilogue();
}

}   //namespace cdb
}   //namespace nx
