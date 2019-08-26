#pragma once

#include <memory>

namespace nx::cloud::storage::service {

namespace conf { class Settings; }

namespace model {

namespace dao {

class AbstractBucketDao;
class AbstractStorageDao;

} // namespace dao

class Database;

class Model
{
public:
    Model(const conf::Settings& settings);
    ~Model();

    void stop();

    Database& database();
    dao::AbstractBucketDao& bucketDao();
    dao::AbstractStorageDao& storageDao();

private:
    std::unique_ptr<Database> m_db;
    std::unique_ptr<dao::AbstractBucketDao> m_bucketDao;
    std::unique_ptr<dao::AbstractStorageDao> m_storageDao;
};

} // namespace model
} // namespace nx::cloud::storage::service
