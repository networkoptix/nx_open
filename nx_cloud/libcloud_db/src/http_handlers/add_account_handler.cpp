/**********************************************************
* 16 may 2015
* a.kolesnikov
***********************************************************/

#include "add_account_handler.h"

#include <utils/common/cpp14.h>
#include <utils/network/http/server/http_stream_socket_server.h>

#include "managers/account_manager.h"
#include "managers/types.h"


namespace nx {
namespace cdb {

const QString AddAccountHttpHandler::HANDLER_PATH = QLatin1String("/add_account");

AddAccountHttpHandler::AddAccountHttpHandler()
:
    AbstractFiniteMsgBodyHttpHandler<data::AccountData>(
        EntityType::account,
        DataActionType::insert )
{
}

AddAccountHttpHandler::~AddAccountHttpHandler()
{
}

void AddAccountHttpHandler::processRequest(
    AuthorizationInfo&& authzInfo,
    data::AccountData&& accountData,
    nx_http::Response* const /*response*/,
    std::function<void( nx_http::StatusCode::Value statusCode )>&& completionHandler )
{
    m_completionHandler = std::move( completionHandler );
    AccountManager::instance()->addAccount(
        authzInfo,
        std::move(accountData),
        std::bind(
            &AddAccountHttpHandler::addAccountDone,
            this,
            std::placeholders::_1 ) );
}

void AddAccountHttpHandler::addAccountDone( ResultCode resultCode )
{
    auto completionHandler = std::move(m_completionHandler);
    completionHandler( resultCodeToHttpStatusCode(resultCode) );
}

}   //cdb
}   //nx
