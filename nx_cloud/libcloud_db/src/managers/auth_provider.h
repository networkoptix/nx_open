/**********************************************************
* Sep 25, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_AUTH_PROVIDER_H
#define NX_AUTH_PROVIDER_H

#include <functional>

#include <QtCore/QByteArray>

#include <cdb/auth_provider.h>

#include "access_control/auth_types.h"
#include "data/auth_data.h"
#include "data/data_filter.h"


namespace nx {
namespace cdb {

namespace conf
{
    class Settings;
}
class AccountManager;
class SystemManager;

//!Provides some temporary hashes which can be used by mediaserver to authenticate requests using cloud account credentials
/*!
    \note These requests are allowed for system only
*/
class AuthenticationProvider
{
public:
    AuthenticationProvider(
        const conf::Settings& settings,
        const AccountManager& accountManager,
        const SystemManager& systemManager);

    //!Returns nonce to be used by mediaserver
    void getCdbNonce(
        const AuthorizationInfo& authzInfo,
        const data::DataFilter& filter,
        std::function<void(api::ResultCode, api::NonceData)> completionHandler);
    //!Returns intermediate HTTP Digest response that can be used to calculate HTTP Digest response with only ha2 known
    /*!
        Usage of intermediate HTTP Digest response is required to keep ha1 inside cdb
    */
    void getAuthenticationResponse(
        const AuthorizationInfo& authzInfo,
        const data::AuthRequest& authRequest,
        std::function<void(api::ResultCode, api::AuthResponse)> completionHandler);

private:
    const conf::Settings& m_settings;
    const AccountManager& m_accountManager;
    const SystemManager& m_systemManager;
};

}   //cdb
}   //nx

#endif  //NX_AUTH_PROVIDER_H
