#include "current_user_rest_handler.h"

#include "nx_ec/data/api_user_data.h"
#include "api/app_server_connection.h"

#include <nx_ec/managers/abstract_user_manager.h>
#include "rest/server/rest_connection_processor.h"

#include <nx_ec/ec_api.h>

#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <network/authenticate_helper.h>
#include <common/common_module.h>

int QnCurrentUserRestHandler::executeGet(
    const QString&,
    const QnRequestParams& params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    ec2::ApiUserData user;

    auto accessRights = owner->accessRights();
    if (accessRights.isNull())
    {
        QnAuthHelper::instance()->doCookieAuthorization(
            "GET", nx_http::getHeaderValue(owner->request().headers, "Cookie"),
            *owner->response(), &accessRights);
    }
    if (accessRights.isNull())
    {
        nx_http::Response response;
        QnAuthHelper::instance()->authenticateByUrl(
            params.value(Qn::URL_QUERY_AUTH_KEY_NAME).toUtf8(), "GET", response, &accessRights);
    }
    if (accessRights.isNull())
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Auth did not pass"));
        return nx_http::StatusCode::unauthorized;
    }

    ec2::AbstractECConnectionPtr ec2Connection = owner->commonModule()->ec2Connection();
    ec2::ApiUserDataList users;
    ec2::ErrorCode errCode = ec2Connection->getUserManager(accessRights)->getUsersSync(&users);
    if (errCode !=  ec2::ErrorCode::ok)
    {
        if (errCode == ec2::ErrorCode::forbidden)
        {
            result.setError(
                QnJsonRestResult::Forbidden,
                lit("Forbidden"));
            return nx_http::StatusCode::forbidden;
        }
        else
        {
            result.setError(
                QnJsonRestResult::CantProcessRequest,
                lit("Internal server error. Can't execute query 'getUsers'"));
            return nx_http::StatusCode::internalServerError;
        }
    }

    auto iter = std::find_if(users.cbegin(), users.cend(), [accessRights](const ec2::ApiUserData& user)
    {
        return user.id == accessRights.userId;
    });

    if (iter == users.cend())
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't determine current user"));
        return nx_http::StatusCode::unauthorized;
    }

    result.setReply(*iter);
    return nx_http::StatusCode::ok;
}
