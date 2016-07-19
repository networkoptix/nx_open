#include "current_user_rest_handler.h"

#include "nx_ec/data/api_user_data.h"
#include "api/app_server_connection.h"

#include <nx_ec/managers/abstract_user_manager.h>
#include "rest/server/rest_connection_processor.h"

#include <nx/network/http/httptypes.h>
#include <network/authenticate_helper.h>

int QnCurrentUserRestHandler::executeGet(const QString &, const QnRequestParams&, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    ec2::ApiUserData user;

    QnUuid userId  = owner->authUserId();
    if (userId.isNull())
    {
        const QString& cookie = QLatin1String(nx_http::getHeaderValue(owner->request().headers, "Cookie"));
        QnAuthHelper::instance()->doCookieAuthorization("GET", cookie.toUtf8(), *owner->response(), &userId);
    }

    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    ec2::ApiUserDataList users;
    ec2::ErrorCode ecode = 
        ec2Connection->getUserManager(Qn::UserAccessData(userId))
                     ->getUsersSync(&users);
    if (ecode !=  ec2::ErrorCode::ok)
    {
        if (ecode == ec2::ErrorCode::forbidden)
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

    auto iter = std::find_if(users.cbegin(), users.cend(), [userId](const ec2::ApiUserData& user)
    {
        return user.id == userId;
    });

    if (iter == users.cend())
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't determine current user"));
        return nx_http::StatusCode::unauthorized;
    }

    result.setReply(*iter);
    return nx_http::StatusCode::ok;
}
