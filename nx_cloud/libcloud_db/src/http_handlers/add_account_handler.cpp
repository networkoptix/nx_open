/**********************************************************
* 16 may 2015
* a.kolesnikov
***********************************************************/

#include "add_account_handler.h"

#include <utils/common/cpp14.h>
#include <utils/network/http/server/http_stream_socket_server.h>
#include <utils/network/http/buffer_source.h>

#include "managers/account_manager.h"
#include "managers/types.h"


namespace cdb_api
{
    const QString AddAccountHttpHandler::HANDLER_PATH = QLatin1String("/add_account");

    void AddAccountHttpHandler::processRequest(
        const AuthorizationInfo& authzInfo,
        const stree::AbstractResourceReader& inputParams,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler )
    {
        AccountData accountData;
        //TODO filling data from inputParams?
        AccountManager::instance()->addAccount(
            authzInfo,
            accountData,
            std::bind(
                &AddAccountHttpHandler::addAccountDone,
                this,
                std::placeholders::_1,
                std::move( completionHandler ) ) );
        //TODO #ak
    }

    void AddAccountHttpHandler::addAccountDone(
        ResultCode resultCode,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )> completionHandler )
    {
        //TODO #ak
        completionHandler(
            resultCode == ResultCode::ok
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::forbidden,
            //std::unique_ptr<nx_http::AbstractMsgBodySource>() );
            std::make_unique<nx_http::BufferSource>(
                "text/html",
                "<html>"
                    "<h1>Hello</h1>"
                    "<body>Hello from base cloud_db add_account handler</body>"
                "</html>\n" ) );
    }
}
