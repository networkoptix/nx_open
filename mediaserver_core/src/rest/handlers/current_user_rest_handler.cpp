#include "current_user_rest_handler.h"

#include <utils/network/tcp_connection_priv.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include "nx_ec/data/api_user_data.h"
#include "api/app_server_connection.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "rest/server/rest_connection_processor.h"

int QnCurrentUserRestHandler::executeGet(const QString &, const QnRequestParams &, QnJsonRestResult &result, const QnRestConnectionProcessor* owner) 
{
    ec2::ApiUserData user;
    QnUuid userId  = owner->authUserId();

    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    QnUserResourceList users;
    if (ec2Connection->getUserManager()->getUsersSync(userId, &users) !=  ec2::ErrorCode::ok)
    {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(tr("Internal server error. Can't execute query 'getUsers'"));
        return CODE_INTERNAL_ERROR;
    }
    if (users.isEmpty()) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(tr("Can't determine current user"));
        return CODE_INTERNAL_ERROR;
    }
    fromResourceToApi(users.first(), user);
    result.setReply(user);
    return CODE_OK;
}
