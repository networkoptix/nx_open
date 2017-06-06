#pragma once

#include "access_control/abstract_authentication_data_provider.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <nx/utils/thread/mutex.h>

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/counter.h>
#include <utils/db/async_sql_query_executor.h>

#include "access_control/auth_types.h"
#include "data/account_data.h"
#include "managers_types.h"


namespace nx {
namespace cdb {

namespace conf {

class Settings;

} // namespace

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

class AbstractTemporaryAccountPasswordManager
{
public:
    virtual ~AbstractTemporaryAccountPasswordManager() = default;

    virtual boost::optional<TemporaryAccountCredentialsEx> getCredentialsByLogin(
        const std::string& login) const = 0;
};

//-------------------------------------------------------------------------------------------------

class TemporaryAccountPasswordManager:
    public AbstractTemporaryAccountPasswordManager,
    public AbstractAuthenticationDataProvider
{
public:
    TemporaryAccountPasswordManager(
        const conf::Settings& settings,
        nx::db::AsyncSqlQueryExecutor* const dbManager) noexcept(false);
    virtual ~TemporaryAccountPasswordManager();

    virtual void authenticateByName(
        const nx_http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const nx::utils::stree::AbstractResourceReader& authSearchInputData,
        nx::utils::stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    void registerTemporaryCredentials(
        const AuthorizationInfo& authzInfo,
        data::TemporaryAccountCredentials tmpPasswordData,
        std::function<void(api::ResultCode)> completionHandler);

    std::string generateRandomPassword() const;
    /**
     * Adds password and password digest.
     * If data->login is empty, random login is generated.
     */
    void addRandomCredentials(data::TemporaryAccountCredentials* const data);

    nx::db::DBResult removeTemporaryPasswordsFromDbByAccountEmail(
        nx::db::QueryContext* const queryContext,
        std::string accountEmail);
    void removeTemporaryPasswordsFromCacheByAccountEmail(
        std::string accountEmail);

    nx::db::DBResult registerTemporaryCredentials(
        nx::db::QueryContext* const queryContext,
        data::TemporaryAccountCredentials tempPasswordData);

    virtual boost::optional<TemporaryAccountCredentialsEx> getCredentialsByLogin(
        const std::string& login) const override;

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

    const conf::Settings& m_settings;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    nx::utils::Counter m_startedAsyncCallsCounter;
    TemporaryCredentialsDictionary m_temporaryCredentials;
    mutable QnMutex m_mutex;

    bool isTemporaryPasswordExpired(
        const TemporaryAccountCredentialsEx& temporaryCredentials) const;
    void removeTemporaryCredentialsFromDbDelayed(
        const TemporaryAccountCredentialsEx& temporaryCredentials);

    nx::db::DBResult fillCache();
    nx::db::DBResult fetchTemporaryPasswords(nx::db::QueryContext* queryContext);

    nx::db::DBResult insertTempPassword(
        nx::db::QueryContext* const queryContext,
        TemporaryAccountCredentialsEx tempPasswordData);
    void saveTempPasswordToCache(TemporaryAccountCredentialsEx tempPasswordData);

    nx::db::DBResult deleteTempPassword(
        nx::db::QueryContext* const queryContext,
        std::string tempPasswordID);
   
    boost::optional<const TemporaryAccountCredentialsEx&> findMatchingCredentials(
        const QnMutexLockerBase& lk,
        const std::string& username,
        std::function<bool(const nx::Buffer&)> checkPasswordHash);
    void runExpirationRulesOnSuccessfulLogin(
        const QnMutexLockerBase& lk,
        const TemporaryAccountCredentialsEx& temporaryCredentials);

    template<typename Index, typename Iterator>
    void updateExpirationRulesAfterSuccessfulLogin(
        const QnMutexLockerBase& lk,
        Index& index,
        Iterator it);
};

} // namespace cdb
} // namespace nx
