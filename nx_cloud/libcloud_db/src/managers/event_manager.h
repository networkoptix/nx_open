/**********************************************************
* May 11, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <map>
#include <memory>

#include <cdb/result_code.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/counter.h>

#include "access_control/auth_types.h"
#include "data/account_data.h"


namespace nx_http
{
    class MessageDispatcher;
    class MultipartMessageBodySource;
}

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
        nx_http::MessageDispatcher* const httpMessageDispatcher);

    void subscribeToEvents(
        const AuthorizationInfo& authzInfo,
        nx::utils::MoveOnlyFunc<
            void(api::ResultCode, std::unique_ptr<nx_http::AbstractMsgBodySource>)
        > completionHandler);

    bool isSystemOnline(const std::string& systemId) const;

private:
    class ServerConnectionContext
    {
    public:
        nx_http::MultipartMessageBodySource* msgBody;

        ServerConnectionContext()
        :
            msgBody(nullptr)
        {
        }
    };

    typedef std::multimap<std::string, ServerConnectionContext> 
        MediaServerConnectionContainer;

    const conf::Settings& m_settings;
    QnCounter m_startedAsyncCallsCounter;
    //map<systemId, context>
    MediaServerConnectionContainer m_activeMediaServerConnections;
    mutable QnMutex m_mutex;

    void beforeMsgBodySourceDestruction(
        MediaServerConnectionContainer::iterator serverConnectionIter);
};

}   //namespace cdb
}   //namespace nx
