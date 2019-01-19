#pragma once

#include <optional>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <nx/network/aio/repetitive_timer.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/counter.h>
#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/thread/mutex.h>

#include "managers_types.h"
#include "../access_control/abstract_authentication_data_provider.h"
#include "../access_control/auth_types.h"
#include "../dao/temporary_credentials_dao.h"
#include "../data/account_data.h"
#include "../settings.h"

namespace nx::cloud::db {

namespace conf { class Settings; } // namespace

//-------------------------------------------------------------------------------------------------

class AbstractTemporaryAccountPasswordManager:
    public AbstractAuthenticationDataProvider
{
public:
    virtual ~AbstractTemporaryAccountPasswordManager() = default;

    virtual void registerTemporaryCredentials(
        const AuthorizationInfo& authzInfo,
        data::TemporaryAccountCredentials tmpPasswordData,
        std::function<void(api::Result)> completionHandler) = 0;

    virtual void addRandomCredentials(
        const std::string& accountEmail,
        data::TemporaryAccountCredentials* const data) = 0;

    virtual nx::sql::DBResult registerTemporaryCredentials(
        nx::sql::QueryContext* const queryContext,
        data::TemporaryAccountCredentials tempPasswordData) = 0;

    virtual std::optional<data::Credentials> fetchTemporaryCredentials(
        nx::sql::QueryContext* const queryContext,
        const data::TemporaryAccountCredentials& tempPasswordData) = 0;

    virtual void updateCredentialsAttributes(
        nx::sql::QueryContext* const queryContext,
        const data::Credentials& credentials,
        const data::TemporaryAccountCredentials& tempPasswordData) = 0;

    /**
     * Throws on failure.
     */
    virtual void removeTemporaryPasswordsFromDbByAccountEmail(
        nx::sql::QueryContext* const queryContext,
        std::string accountEmail) = 0;

    virtual void removeTemporaryPasswordsFromCacheByAccountEmail(
        std::string accountEmail) = 0;

    virtual std::optional<data::TemporaryAccountCredentialsEx> getCredentialsByLogin(
        const std::string& login) const = 0;

    virtual bool authorize(
        const std::string& credentialsId,
        const nx::utils::stree::AbstractResourceReader& inputData) const = 0;
};

//-------------------------------------------------------------------------------------------------

class TemporaryAccountPasswordManager:
    public AbstractTemporaryAccountPasswordManager
{
public:
    TemporaryAccountPasswordManager(
        const conf::AccountManager& settings,
        const nx::utils::stree::ResourceNameSet& attributeNameset,
        nx::sql::AsyncSqlQueryExecutor* const dbManager,
        dao::AbstractTemporaryCredentialsDao* const dao) noexcept(false);
    virtual ~TemporaryAccountPasswordManager();

    virtual void authenticateByName(
        const nx::network::http::StringType& username,
        const std::function<bool(const nx::Buffer&)>& validateHa1Func,
        nx::utils::stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::Result)> completionHandler) override;

    virtual void registerTemporaryCredentials(
        const AuthorizationInfo& authzInfo,
        data::TemporaryAccountCredentials tmpPasswordData,
        std::function<void(api::Result)> completionHandler) override;

    /**
     * Adds password and password digest.
     * If data->login is empty, random login is generated.
     */
    virtual void addRandomCredentials(
        const std::string& accountEmail,
        data::TemporaryAccountCredentials* const data) override;

    virtual void removeTemporaryPasswordsFromDbByAccountEmail(
        nx::sql::QueryContext* const queryContext,
        std::string accountEmail) override;
    
    virtual void removeTemporaryPasswordsFromCacheByAccountEmail(
        std::string accountEmail) override;

    virtual nx::sql::DBResult registerTemporaryCredentials(
        nx::sql::QueryContext* const queryContext,
        data::TemporaryAccountCredentials tempPasswordData) override;

    virtual std::optional<data::Credentials> fetchTemporaryCredentials(
        nx::sql::QueryContext* const queryContext,
        const data::TemporaryAccountCredentials& tempPasswordData) override;

    virtual void updateCredentialsAttributes(
        nx::sql::QueryContext* const queryContext,
        const data::Credentials& credentials,
        const data::TemporaryAccountCredentials& tempPasswordData) override;

    virtual std::optional<data::TemporaryAccountCredentialsEx> getCredentialsByLogin(
        const std::string& login) const override;

    virtual bool authorize(
        const std::string& credentialsId,
        const nx::utils::stree::AbstractResourceReader& inputData) const override;

    std::string generateRandomPassword() const;

private:
    typedef boost::multi_index::multi_index_container<
        data::TemporaryAccountCredentialsEx,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<boost::multi_index::member<
                data::TemporaryAccountCredentialsEx,
                std::string,
                &data::TemporaryAccountCredentialsEx::id>>,
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                data::TemporaryAccountCredentials,
                std::string,
                &data::TemporaryAccountCredentials::login>>,
            boost::multi_index::ordered_non_unique<boost::multi_index::member<
                data::TemporaryAccountCredentials,
                std::string,
                &data::TemporaryAccountCredentials::accountEmail>>
        >
    > TemporaryCredentialsDictionary;

    constexpr static const int kIndexById = 0;
    constexpr static const int kIndexByLogin = 1;
    constexpr static const int kIndexByAccountEmail = 2;

    const conf::AccountManager m_settings;
    const nx::utils::stree::ResourceNameSet& m_attributeNameset;
    nx::sql::AsyncSqlQueryExecutor* const m_dbManager;
    nx::utils::Counter m_startedAsyncCallsCounter;
    TemporaryCredentialsDictionary m_temporaryCredentials;
    mutable QnMutex m_mutex;
    dao::AbstractTemporaryCredentialsDao* m_dao = nullptr;
    nx::network::aio::Timer m_removeExpiredCredentialsTimer;
    bool m_terminated = false;

    bool isTemporaryPasswordExpired(
        const data::TemporaryAccountCredentialsEx& temporaryCredentials) const;

    void removeTemporaryCredentialsFromDbDelayed(
        const data::TemporaryAccountCredentialsEx& temporaryCredentials);

    void deleteExpiredCredentials();
    void fillCache();

    void schedulePeriodicRemovalOfExpiredCredentials();
    void doPeriodicRemovalOfExpiredCredentials();

    nx::sql::DBResult insertTempPassword(
        nx::sql::QueryContext* const queryContext,
        data::TemporaryAccountCredentialsEx tempPasswordData);

    void saveTempPasswordToCache(data::TemporaryAccountCredentialsEx tempPasswordData);

    const data::TemporaryAccountCredentialsEx* findMatchingCredentials(
        const QnMutexLockerBase& lk,
        const std::string& username,
        std::function<bool(const nx::Buffer&)> checkPasswordHash,
        api::Result* authResultCode);

    void runExpirationRulesOnSuccessfulLogin(
        const QnMutexLockerBase& lk,
        const data::TemporaryAccountCredentialsEx& temporaryCredentials);

    template<typename Index, typename Iterator>
    void updateExpirationRulesAfterSuccessfulLogin(
        const QnMutexLockerBase& lk,
        Index& index,
        Iterator it);

    void updateCredentialsInDbAsync(
        const data::TemporaryAccountCredentials& tempPasswordData);

    void updateCredentialsInCache(
        const data::Credentials& credentials,
        const data::TemporaryAccountCredentials& tempPasswordData);
};

} // namespace nx::cloud::db
