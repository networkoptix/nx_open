#include "current_user_rest_handler.h"

#include "nx_ec/data/api_user_data.h"
#include "api/app_server_connection.h"

#include <nx_ec/managers/abstract_user_manager.h>
#include "rest/server/rest_connection_processor.h"

#include <nx/network/http/httptypes.h>

int QnCurrentUserRestHandler::executeGet(const QString &, const QnRequestParams &, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    ec2::ApiUserData user;
    QnUuid userId  = owner->authUserId();

    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    ec2::ApiUserDataList users;
    if (ec2Connection->getUserManager()->getUsersSync(&users) !=  ec2::ErrorCode::ok)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error. Can't execute query 'getUsers'"));
        return nx_http::StatusCode::internalServerError;
    }

    auto iter = std::find_if(users.cbegin(), users.cend(), [userId](const ec2::ApiUserData& user)
    {
        return user.id == userId;
    });

    if (iter == users.cend())
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't determine current user"));
        return nx_http::StatusCode::internalServerError;
    }

    result.setReply(*iter);
    return nx_http::StatusCode::ok;
}
