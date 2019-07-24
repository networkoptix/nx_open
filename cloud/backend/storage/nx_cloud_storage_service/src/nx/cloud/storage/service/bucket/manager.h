#pragma once

#include <nx/utils/basic_factory.h>
#include <nx/network/cloud/storage/service/api/result.h>
#include <nx/network/cloud/storage/service/api/add_bucket.h>
#include <nx/network/cloud/storage/service/api/bucket.h>

namespace nx::cloud {
namespace storage::service {

namespace conf {

struct Aws;
class Settings;

} // namespace conf

class Database;

namespace bucket {

class AbstractManager
{
public:
    virtual ~AbstractManager() = default;

    virtual void addBucket(
        const api::AddBucketRequest& request,
        const nx::utils::MoveOnlyFunc<void(api::Result, api::Error)> handler) = 0;

    virtual void listBuckets(
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::Bucket>)> handler) = 0;

    virtual void removeBucket(
        const std::string& bucketName,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler) = 0;
};

//-------------------------------------------------------------------------------------------------
// Manager

class Manager:
    public AbstractManager
{
public:
    Manager(const conf::Aws& settings, Database* database);

    virtual void addBucket(
        const api::AddBucketRequest& request,
        const nx::utils::MoveOnlyFunc<void(api::Result, api::Error)> handler) override;

    virtual void listBuckets(
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::Bucket>)> handler) override;

    virtual void removeBucket(
        const std::string& bucketName,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler) override;

private:
    /*const conf::Aws& m_settings;
    Database* m_database;*/
};

//-------------------------------------------------------------------------------------------------
// ManagerFactory

using FactoryType = std::unique_ptr<AbstractManager>(const conf::Settings&, Database*);

class ManagerFactory:
    public nx::utils::BasicFactory<FactoryType>
{
    using base_type = nx::utils::BasicFactory<FactoryType>;

public:
    ManagerFactory();

    static ManagerFactory& instance();

private:
    std::unique_ptr<AbstractManager> defaultFactoryFunction(
        const conf::Settings& settings, Database* database);
};

} // namespace bucket
} // namespace storage::service
} // namespace nx::cloud
