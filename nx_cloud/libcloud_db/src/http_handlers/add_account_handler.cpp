/**********************************************************
* 16 may 2015
* a.kolesnikov
***********************************************************/

#include "add_account_handler.h"

#include <utils/common/cpp14.h>
#include <utils/network/http/server/http_stream_socket_server.h>

#include "managers/account_manager.h"
#include "managers/types.h"


namespace cdb
{
    const QString AddAccountHttpHandler::HANDLER_PATH = QLatin1String("/add_account");

    AddAccountHttpHandler::AddAccountHttpHandler()
    :
        AbstractFiniteMsgBodyHttpHandler<data::AccountData, data::EmailVerificationCode>(
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
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            const data::EmailVerificationCode& output )>&& completionHandler )
    {
        m_completionHandler = std::move( completionHandler );
        AccountManager::instance()->addAccount(
            authzInfo,
            std::move(accountData),
            std::bind(
                &AddAccountHttpHandler::addAccountDone,
                this,
                std::placeholders::_1,
                std::placeholders::_2 ) );
    }

    void AddAccountHttpHandler::addAccountDone(
        ResultCode resultCode,
        data::EmailVerificationCode outData )
    {
        auto completionHandler = std::move(m_completionHandler);
        completionHandler(
            resultCode == ResultCode::ok
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::forbidden,
            std::move( outData ) );
    }
}
