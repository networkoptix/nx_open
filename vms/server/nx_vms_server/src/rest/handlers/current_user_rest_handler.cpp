#include "current_user_rest_handler.h"

#include <api/abstract_connection.h>
#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <network/universal_tcp_listener.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <rest/server/rest_connection_processor.h>

#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <nx/vms/api/data/user_data.h>

int QnCurrentUserRestHandler::executeGet(
    const QString&,
    const QnRequestParams& params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    const auto authenticator = QnUniversalTcpListener::authenticator(owner->owner());
    const auto clientIp = owner->getForeignAddress().address;
    auto accessRights = owner->accessRights();

    if (accessRights.isNull())
        accessRights = authenticator->tryCookie(owner->request()).access;

    if (accessRights.isNull())
    {
        nx::network::http::Response response;
        authenticator->tryAuthRecord(
            clientIp, params.value(Qn::URL_QUERY_AUTH_KEY_NAME).toUtf8(),
            "GET", response, &accessRights);
    }

    if (accessRights.isNull())
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Auth did not pass"));
        return nx::network::http::StatusCode::unauthorized;
    }

    ec2::AbstractECConnectionPtr ec2Connection = owner->commonModule()->ec2Connection();
    nx::vms::api::UserDataList users;
    ec2::ErrorCode errCode = ec2Connection->getUserManager(accessRights)->getUsersSync(&users);
    if (errCode !=  ec2::ErrorCode::ok)
    {
        if (errCode == ec2::ErrorCode::forbidden)
        {
            result.setError(
                QnJsonRestResult::Forbidden,
                lit("Forbidden"));
            return nx::network::http::StatusCode::forbidden;
        }
        else
        {
            result.setError(
                QnJsonRestResult::CantProcessRequest,
                lit("Internal server error. Can't execute query 'getUsers'"));
            return nx::network::http::StatusCode::internalServerError;
        }
    }

    const auto iter = std::find_if(users.cbegin(), users.cend(),
        [accessRights](const nx::vms::api::UserData& user)
        {
            return user.id == accessRights.userId;
        });

    if (iter == users.cend())
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't determine current user"));
        return nx::network::http::StatusCode::unauthorized;
    }

    result.setReply(*iter);
    return nx::network::http::StatusCode::ok;
}
