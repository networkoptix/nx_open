/**********************************************************
* Aug 8, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <list>
#include <map>
#include <memory>

#include <cdb/result_code.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

#include "access_control/auth_types.h"


namespace nx_http {
class HttpServerConnection;
class MessageDispatcher;
}   // namespace nx_http

namespace nx {
namespace cdb {

class AuthorizationManager;
namespace conf {
class Settings;
}   //namespace conf

namespace ec2 {

class TransactionTransport;

/** Managers ec2 transaction connections from mediaservers */
class ConnectionManager
{
public:
    ConnectionManager(const conf::Settings& settings);
    virtual ~ConnectionManager();

    void registerHttpHandlers(
        const AuthorizationManager& authorizationManager,
        nx_http::MessageDispatcher* const httpMessageDispatcher);

    /** mediaserver calls this method to open 2-way transaction exchange channel */
    void createTransactionConnection(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource)
        > completionHandler);
    /** mediaserver uses this method to push transactions */
    void pushTransaction(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBodyDataSource)
        > completionHandler);

private:
    const conf::Settings& m_settings;
    // map<pair<systemid, peer id>, connection>
    std::map<std::pair<QString, QnUuid>, std::unique_ptr<TransactionTransport>> m_connections;
    std::map<TransactionTransport*, std::unique_ptr<TransactionTransport>> m_connectionsToRemove;
    QnMutex m_mutex;

    void removeConnection(TransactionTransport* transport);
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
