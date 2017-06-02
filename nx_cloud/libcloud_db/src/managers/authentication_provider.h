#pragma once

#include <functional>

#include <QtCore/QByteArray>

#include <cdb/auth_provider.h>

#include "access_control/auth_types.h"
#include "dao/abstract_user_authentication_data_object.h"
#include "data/account_data.h"
#include "data/auth_data.h"
#include "data/data_filter.h"
#include "system_sharing_manager.h"

namespace nx {
namespace cdb {

namespace conf {
class Settings;
} // namespace conf

class AbstractAccountManager;
class AbstractTemporaryAccountPasswordManager;

/**
 * Provides some temporary hashes which can be used by mediaserver 
 * to authenticate requests using cloud account credentials.
 * NOTE: These requests are allowed for system only.
 */
class AuthenticationProvider:
    public AbstractSystemSharingExtension
{
public:
    AuthenticationProvider(
        const conf::Settings& settings,
        const AbstractAccountManager& accountManager,
        AbstractSystemSharingManager* systemSharingManager,
        const AbstractTemporaryAccountPasswordManager& temporaryAccountCredentialsManager);
    virtual ~AuthenticationProvider();

    virtual nx::db::DBResult afterSharingSystem(
        nx::db::QueryContext* const queryContext,
        const api::SystemSharing& sharing,
        SharingType sharingType) override;

    /**
     * @return nonce to be used by mediaserver.
     */
    void getCdbNonce(
        const AuthorizationInfo& authzInfo,
        const data::DataFilter& filter,
        std::function<void(api::ResultCode, api::NonceData)> completionHandler);
    /**
     * @return intermediate HTTP Digest response that can be used to calculate 
     *   HTTP Digest response with only ha2 known.
     * Usage of intermediate HTTP Digest response is required to keep ha1 inside cdb.
     */
    void getAuthenticationResponse(
        const AuthorizationInfo& authzInfo,
        const data::AuthRequest& authRequest,
        std::function<void(api::ResultCode, api::AuthResponse)> completionHandler);

private:
    struct AccountWithEffectivePassword
    {
        data::AccountData account;
        std::string passwordHa1;
    };

    const conf::Settings& m_settings;
    const AbstractAccountManager& m_accountManager;
    AbstractSystemSharingManager* m_systemSharingManager;
    const AbstractTemporaryAccountPasswordManager& m_temporaryAccountCredentialsManager;
    std::unique_ptr<dao::AbstractUserAuthentication> m_authenticationDataObject;

    boost::optional<AccountWithEffectivePassword> 
        getAccountByLogin(const std::string& login) const;
    bool validateNonce(
        const std::string& nonce,
        const std::string& systemId) const;
    api::AuthResponse prepareResponse(
        std::string nonce,
        api::SystemSharingEx systemSharing,
        const std::string& passwordHa1) const;

    std::string fetchOrCreateNonce(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId);
    api::AuthInfo generateAuthRecord(
        const std::string& systemId,
        const api::AccountData& account,
        const std::string& nonce);
    void removeExpiredRecords(
        std::vector<api::AuthInfo>* userAuthenticationRecords);
    void generateUpdateUserAuthInfoTransaction(
        const std::vector<api::AuthInfo>& userAuthenticationRecords);
};

} // namespace cdb
} // namespace nx
