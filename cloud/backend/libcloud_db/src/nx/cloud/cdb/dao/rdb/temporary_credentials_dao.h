#pragma once

#include "../temporary_credentials_dao.h"

namespace nx::cloud::db::dao::rdb {

class TemporaryCredentialsDao:
    public AbstractTemporaryCredentialsDao
{
public:
    virtual void insert(
        nx::sql::QueryContext* queryContext,
        const nx::utils::stree::ResourceNameSet& attributeNameset,
        const data::TemporaryAccountCredentialsEx& tempPasswordData) override;

    virtual void removeByAccountEmail(
        nx::sql::QueryContext* queryContext,
        const std::string& email) override;

    virtual std::optional<data::Credentials> find(
        nx::sql::QueryContext* queryContext,
        const nx::utils::stree::ResourceNameSet& attributeNameset,
        const data::TemporaryAccountCredentials& filter) override;

    virtual void update(
        nx::sql::QueryContext* queryContext,
        const nx::utils::stree::ResourceNameSet& attributeNameset,
        const data::Credentials& credentials,
        const data::TemporaryAccountCredentials& tempPasswordData) override;

    virtual void deleteById(
        nx::sql::QueryContext* queryContext,
        const std::string& id) override;

    virtual void deleteExpiredCredentials(nx::sql::QueryContext* queryContext) override;

    virtual std::vector<data::TemporaryAccountCredentialsEx> fetchAll(
        nx::sql::QueryContext* queryContext,
        const nx::utils::stree::ResourceNameSet& attributeNameset) override;

private:
    void parsePasswordString(
        const std::string& passwordString,
        std::string* passwordHa1,
        std::string* password);

    std::string preparePasswordString(
        const std::string& passwordHa1,
        const std::string& password);
};

} // namespace nx::cloud::db::dao::rdb
