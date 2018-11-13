#pragma once

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <nx/utils/counter.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/thread/mutex.h>

#include "managers_types.h"
#include "../access_control/abstract_authentication_data_provider.h"
#include "../access_control/auth_types.h"
#include "../data/account_data.h"

namespace nx {
namespace cdb {

namespace conf { class Settings; } // namespace

class TemporaryAccountCredentialsEx:
    public data::TemporaryAccountCredentials
{
public:
    std::string id;
    std::string accessRightsStr;

    TemporaryAccountCredentialsEx() {}
    TemporaryAccountCredentialsEx(const TemporaryAccountCredentialsEx& right):
        data::TemporaryAccountCredentials(right),
        id(right.id),
        accessRightsStr(right.accessRightsStr)
    {
    }
    TemporaryAccountCredentialsEx(data::TemporaryAccountCredentials&& right):
        data::TemporaryAccountCredentials(std::move(right))
    {
    }
};

#define TemporaryAccountCredentialsEx_Fields \
    TemporaryAccountCredentials_Fields (id)(accessRightsStr)

//-------------------------------------------------------------------------------------------------
namespace data {

class Credentials
{
public:
    std::string login;
    std::string password;
};

} // namespace data

class AbstractTemporaryAccountPasswordManager:
    public AbstractAuthenticationDataProvider
{
public:
    virtual ~AbstractTemporaryAccountPasswordManager() = default;

    virtual void registerTemporaryCredentials(
        const AuthorizationInfo& authzInfo,
        data::TemporaryAccountCredentials tmpPasswordData,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    virtual void addRandomCredentials(
        const std::string& accountEmail,
        data::TemporaryAccountCredentials* const data) = 0;

    virtual nx::sql::DBResult registerTemporaryCredentials(
        nx::sql::QueryContext* const queryContext,
        data::TemporaryAccountCredentials tempPasswordData) = 0;

    virtual nx::sql::DBResult fetchTemporaryCredentials(
        nx::sql::QueryContext* const queryContext,
        const data::TemporaryAccountCredentials& tempPasswordData,
        data::Credentials* credentials) = 0;

    virtual nx::sql::DBResult updateCredentialsAttributes(
        nx::sql::QueryContext* const queryContext,
        const data::Credentials& credentials,
        const data::TemporaryAccountCredentials& tempPasswordData) = 0;

    virtual nx::sql::DBResult removeTemporaryPasswordsFromDbByAccountEmail(
        nx::sql::QueryContext* const queryContext,
        std::string accountEmail) = 0;

    virtual void removeTemporaryPasswordsFromCacheByAccountEmail(
        std::string accountEmail) = 0;

    virtual boost::optional<TemporaryAccountCredentialsEx> getCredentialsByLogin(
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
        const nx::utils::stree::ResourceNameSet& attributeNameset,
        nx::sql::AsyncSqlQueryExecutor* const dbManager) noexcept(false);
    virtual ~TemporaryAccountPasswordManager();

    virtual void authenticateByName(
        const nx::network::http::StringType& username,
        const std::function<bool(const nx::Buffer&)>& validateHa1Func,
        nx::utils::stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    virtual void registerTemporaryCredentials(
        const AuthorizationInfo& authzInfo,
        data::TemporaryAccountCredentials tmpPasswordData,
        std::function<void(api::ResultCode)> completionHandler) override;

    /**
     * Adds password and password digest.
     * If data->login is empty, random login is generated.
     */
    virtual void addRandomCredentials(
        const std::string& accountEmail,
        data::TemporaryAccountCredentials* const data) override;

    virtual nx::sql::DBResult removeTemporaryPasswordsFromDbByAccountEmail(
        nx::sql::QueryContext* const queryContext,
        std::string accountEmail) override;
    virtual void removeTemporaryPasswordsFromCacheByAccountEmail(
        std::string accountEmail) override;

    virtual nx::sql::DBResult registerTemporaryCredentials(
        nx::sql::QueryContext* const queryContext,
        data::TemporaryAccountCredentials tempPasswordData) override;

    virtual nx::sql::DBResult fetchTemporaryCredentials(
        nx::sql::QueryContext* const queryContext,
        const data::TemporaryAccountCredentials& tempPasswordData,
        data::Credentials* credentials) override;

    virtual nx::sql::DBResult updateCredentialsAttributes(
        nx::sql::QueryContext* const queryContext,
        const data::Credentials& credentials,
        const data::TemporaryAccountCredentials& tempPasswordData) override;

    virtual boost::optional<TemporaryAccountCredentialsEx> getCredentialsByLogin(
        const std::string& login) const override;

    virtual bool authorize(
        const std::string& credentialsId,
        const nx::utils::stree::AbstractResourceReader& inputData) const override;

    std::string generateRandomPassword() const;

private:
    typedef boost::multi_index::multi_index_container<
        TemporaryAccountCredentialsEx,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<boost::multi_index::member<
                TemporaryAccountCredentialsEx,
                std::string,
                &TemporaryAccountCredentialsEx::id>>,
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

    const nx::utils::stree::ResourceNameSet& m_attributeNameset;
    nx::sql::AsyncSqlQueryExecutor* const m_dbManager;
    nx::utils::Counter m_startedAsyncCallsCounter;
    TemporaryCredentialsDictionary m_temporaryCredentials;
    mutable QnMutex m_mutex;

    bool isTemporaryPasswordExpired(
        const TemporaryAccountCredentialsEx& temporaryCredentials) const;
    void removeTemporaryCredentialsFromDbDelayed(
        const TemporaryAccountCredentialsEx& temporaryCredentials);

    void deleteExpiredCredentials();
    nx::sql::DBResult fillCache();
    nx::sql::DBResult fetchTemporaryPasswords(nx::sql::QueryContext* queryContext);

    nx::sql::DBResult insertTempPassword(
        nx::sql::QueryContext* const queryContext,
        TemporaryAccountCredentialsEx tempPasswordData);
    void saveTempPasswordToCache(TemporaryAccountCredentialsEx tempPasswordData);

    nx::sql::DBResult deleteTempPassword(
        nx::sql::QueryContext* const queryContext,
        std::string tempPasswordID);

    boost::optional<const TemporaryAccountCredentialsEx&> findMatchingCredentials(
        const QnMutexLockerBase& lk,
        const std::string& username,
        std::function<bool(const nx::Buffer&)> checkPasswordHash,
        api::ResultCode* authResultCode);
    void runExpirationRulesOnSuccessfulLogin(
        const QnMutexLockerBase& lk,
        const TemporaryAccountCredentialsEx& temporaryCredentials);

    template<typename Index, typename Iterator>
    void updateExpirationRulesAfterSuccessfulLogin(
        const QnMutexLockerBase& lk,
        Index& index,
        Iterator it);

    void parsePasswordString(
        const std::string& passwordString,
        std::string* passwordHa1,
        std::string* password);
    std::string preparePasswordString(
        const std::string& passwordHa1,
        const std::string& password);

    void updateCredentialsInCache(
        const data::Credentials& credentials,
        const data::TemporaryAccountCredentials& tempPasswordData);
};

} // namespace cdb
} // namespace nx
