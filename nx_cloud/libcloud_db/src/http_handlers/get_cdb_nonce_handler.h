/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#ifndef GET_CDB_NONCE_HANDLER_H
#define GET_CDB_NONCE_HANDLER_H

#include <cloud_db_client/src/data/auth_data.h>

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "managers/auth_provider.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {


class GetCdbNonceHandler
:
    public AbstractFiniteMsgBodyHttpHandler<void, api::NonceData>
{
public:
    static const QString kHandlerPath;

    GetCdbNonceHandler(
        AuthenticationProvider* const authProvider,
        const AuthorizationManager& authorizationManager )
    :
        AbstractFiniteMsgBodyHttpHandler<void, api::NonceData>(
            EntityType::account,
            DataActionType::fetch,
            authorizationManager,
            std::bind(
                &AuthenticationProvider::getCdbNonce, authProvider,
                std::placeholders::_1, std::placeholders::_2 ) )
    {
    }
};

}   //cdb
}   //nx

#endif  //GET_CDB_NONCE_HANDLER_H
