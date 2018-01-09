/**********************************************************
* May 11, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <map>
#include <memory>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <nx/cloud/cdb/api/result_code.h>
#include <nx/network/aio/timer.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/counter.h>

#include "../access_control/auth_types.h"
#include "../data/account_data.h"


namespace nx {
namespace network {
namespace http {

class HttpServerConnection;
class MessageDispatcher;
class MultipartMessageBodySource;

} // namespace http
} // namespace network
} // namespace nx

namespace nx {
namespace cdb {

namespace conf
{
    class Settings;
}
class AuthorizationManager;

class EventManager
{
public:
    EventManager(const conf::Settings& settings);
    virtual ~EventManager();

    void registerHttpHandlers(
        const AuthorizationManager& authorizationManager,
        nx::network::http::server::rest::MessageDispatcher* const httpMessageDispatcher);

    void subscribeToEvents(
        nx::network::http::HttpServerConnection* connection,
        const AuthorizationInfo& authzInfo,
        nx::utils::MoveOnlyFunc<
            void(api::ResultCode, std::unique_ptr<nx::network::http::AbstractMsgBodySource>)
        > completionHandler);

    bool isSystemOnline(const std::string& systemId) const;

private:
    class ServerConnectionContext
    {
    public:
        const nx::network::http::HttpServerConnection* httpConnection;
        nx::network::http::MultipartMessageBodySource* msgBody;
        std::unique_ptr<nx::network::aio::Timer> timer;
        const std::string systemId;

        ServerConnectionContext(
            const nx::network::http::HttpServerConnection* _httpConnection,
            const std::string& _systemId)
        :
            httpConnection(_httpConnection),
            msgBody(nullptr),
            systemId(_systemId)
        {
        }
    };

    typedef boost::multi_index::multi_index_container<
        ServerConnectionContext,
        boost::multi_index::indexed_by<
            //indexing by http connection
            boost::multi_index::ordered_unique<
                boost::multi_index::member<
                    ServerConnectionContext,
                    const nx::network::http::HttpServerConnection*,
                    &ServerConnectionContext::httpConnection >>,
            //indexing by system id
            boost::multi_index::ordered_non_unique<
                boost::multi_index::member<
                    ServerConnectionContext,
                    const std::string,
                    &ServerConnectionContext::systemId >>
        >
    > MediaServerConnectionContainer;

    constexpr static const int kServerContextByConnectionIndex = 0;
    constexpr static const int kServerContextBySystemIdIndex = 1;

    const conf::Settings& m_settings;
    nx::utils::Counter m_startedAsyncCallsCounter;
    MediaServerConnectionContainer m_activeMediaServerConnections;
    mutable QnMutex m_mutex;

    void beforeMsgBodySourceDestruction(
        nx::network::http::HttpServerConnection* connection);
    void onConnectionToPeerLost(
        MediaServerConnectionContainer::iterator serverConnectionIter);
    void onMediaServerIdlePeriodExpired(
        MediaServerConnectionContainer::iterator serverConnectionIter);
};

}   //namespace cdb
}   //namespace nx
