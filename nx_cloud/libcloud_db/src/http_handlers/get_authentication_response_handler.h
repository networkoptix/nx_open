/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#ifndef GET_AUTHENTICATION_RESPONSE_HANDLER_H
#define GET_AUTHENTICATION_RESPONSE_HANDLER_H

#include <cloud_db_client/src/data/auth_data.h>

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "data/auth_data.h"
#include "managers/auth_provider.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {


class GetAuthenticationResponseHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::AuthRequest, api::AuthResponse>
{
public:
    static const QString kHandlerPath;

    GetAuthenticationResponseHandler(
        AuthenticationProvider* const authProvider,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFiniteMsgBodyHttpHandler<data::AuthRequest, api::AuthResponse>(
            EntityType::account,
            DataActionType::fetch,
            authorizationManager,
            std::bind(
                &AuthenticationProvider::getAuthenticationResponse, authProvider,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
    }
};

}  //cdb
}  //nx

#endif  //GET_AUTHENTICATION_RESPONSE_HANDLER_H
