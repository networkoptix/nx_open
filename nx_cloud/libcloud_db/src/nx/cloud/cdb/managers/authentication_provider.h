#pragma once

#include <functional>

#include <QtCore/QByteArray>

#include <nx/cloud/cdb/api/auth_provider.h>
#include <nx/utils/db/async_sql_query_executor.h>

#include "account_manager.h"
#include "system_sharing_manager.h"
#include "../access_control/auth_types.h"
#include "../dao/abstract_user_authentication_data_object.h"
#include "../data/account_data.h"
#include "../data/auth_data.h"
#include "../data/data_filter.h"

namespace nx {
namespace cdb {

namespace conf { class Settings; }
namespace ec2 { class AbstractVmsP2pCommandBus; }

class AbstractTemporaryAccountPasswordManager;

/**
 * Provides some temporary hashes which can be used by mediaserver 
 * to authenticate requests using cloud account credentials.
 * NOTE: These requests are allowed for system only.
 */
class AuthenticationProvider:
    public AbstractSystemSharingExtension,
    public AbstractAccountManagerExtension
{
public:
    AuthenticationProvider(
        const conf::Settings& settings,
        nx::utils::db::AsyncSqlQueryExecutor* sqlQueryExecutor,
        AbstractAccountManager* accountManager,
        AbstractSystemSharingManager* systemSharingManager,
        const AbstractTemporaryAccountPasswordManager& temporaryAccountCredentialsManager,
        ec2::AbstractVmsP2pCommandBus* vmsP2pCommandBus);
    virtual ~AuthenticationProvider();

    virtual nx::utils::db::DBResult afterSharingSystem(
        nx::utils::db::QueryContext* const queryContext,
        const api::SystemSharing& sharing,
        SharingType sharingType) override;
    
    virtual void afterUpdatingAccountPassword(
        nx::utils::db::QueryContext* const queryContext,
        const api::AccountData& account) override;

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
    nx::utils::db::AsyncSqlQueryExecutor* m_sqlQueryExecutor;
    AbstractAccountManager* m_accountManager;
    AbstractSystemSharingManager* m_systemSharingManager;
    const AbstractTemporaryAccountPasswordManager& m_temporaryAccountCredentialsManager;
    std::unique_ptr<dao::AbstractUserAuthentication> m_authenticationDataObject;
    ec2::AbstractVmsP2pCommandBus* m_vmsP2pCommandBus;

    boost::optional<AccountWithEffectivePassword> 
        getAccountByLogin(const std::string& login) const;
    nx::utils::db::DBResult validateNonce(
        nx::utils::db::QueryContext* queryContext,
        const std::string& nonce,
        const std::string& systemId,
        std::shared_ptr<bool> isNonceValid);
    std::tuple<api::ResultCode, api::AuthResponse>
        prepareAuthenticationResponse(
            const std::string& systemId,
            const data::AuthRequest& authRequest);
    api::AuthResponse prepareResponse(
        std::string nonce,
        api::SystemSharingEx systemSharing,
        const std::string& passwordHa1) const;

    std::string fetchOrCreateNonce(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId);
    void addUserAuthRecord(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& vmsUserId,
        const api::AccountData& account,
        const std::string& nonce);
    api::AuthInfoRecord generateAuthRecord(
        const api::AccountData& account,
        const std::string& nonce);
    void removeExpiredRecords(api::AuthInfo* userAuthenticationRecords);
    void generateUpdateUserAuthInfoTransaction(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& vmsUserId,
        const api::AuthInfo& userAuthenticationRecords);
};

} // namespace cdb
} // namespace nx
