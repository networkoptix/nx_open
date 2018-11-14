#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/sql/query_context.h>
#include <nx/utils/basic_factory.h>

#include "../data/account_data.h"

namespace nx::cloud::db::data {

// TODO: #ak Refactor it out.
class Credentials
{
public:
    std::string login;
    std::string password;
};

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

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (TemporaryAccountCredentialsEx),
    (sql_record))

} // namespace nx::cloud::db::data

//-------------------------------------------------------------------------------------------------

namespace nx::cloud::db::dao {

class AbstractTemporaryCredentialsDao
{
public:
    virtual ~AbstractTemporaryCredentialsDao() = default;

    virtual void insert(
        nx::sql::QueryContext* queryContext,
        const nx::utils::stree::ResourceNameSet& attributeNameset,
        const data::TemporaryAccountCredentialsEx& tempPasswordData) = 0;

    virtual void removeByAccountEmail(
        nx::sql::QueryContext* queryContext,
        const std::string& email) = 0;

    virtual std::optional<data::Credentials> find(
        nx::sql::QueryContext* queryContext,
        const nx::utils::stree::ResourceNameSet& attributeNameset,
        const data::TemporaryAccountCredentials& filter) = 0;

    virtual void update(
        nx::sql::QueryContext* queryContext,
        const nx::utils::stree::ResourceNameSet& attributeNameset,
        const data::Credentials& credentials,
        const data::TemporaryAccountCredentials& tempPasswordData) = 0;

    virtual void deleteById(
        nx::sql::QueryContext* queryContext,
        const std::string& id) = 0;

    virtual void deleteExpiredCredentials(
        nx::sql::QueryContext* queryContext) = 0;

    virtual std::vector<data::TemporaryAccountCredentialsEx> fetchAll(
        nx::sql::QueryContext* queryContext,
        const nx::utils::stree::ResourceNameSet& attributeNameset) = 0;
};

//-------------------------------------------------------------------------------------------------

using TemporaryCredentialsDaoFactoryFunction =
    std::unique_ptr<AbstractTemporaryCredentialsDao>();

class TemporaryCredentialsDaoFactory:
    public nx::utils::BasicFactory<TemporaryCredentialsDaoFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<TemporaryCredentialsDaoFactoryFunction>;

public:
    TemporaryCredentialsDaoFactory();

    static TemporaryCredentialsDaoFactory& instance();

private:
    std::unique_ptr<AbstractTemporaryCredentialsDao> defaultFactoryFunction();
};

} // namespace nx::cloud::db::dao
