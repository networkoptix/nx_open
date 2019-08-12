#include "model.h"

#include "nx/cloud/storage/service/settings.h"
#include "database.h"
#include "dao/bucket_dao.h"
#include "dao/storage_dao.h"

namespace nx::cloud::storage::service::model {

Model::Model(const conf::Settings& settings):
    m_database(std::make_unique<Database>(settings)),
    m_bucketDao(dao::BucketDaoFactory::instance().create()),
    m_storageDao(dao::StorageDaoFactory::instance().create())
{
}

Model::~Model()
{
    stop();
}

void Model::stop()
{
    m_database->stop();
}

Database& Model::database()
{
    return *m_database;
}

dao::AbstractBucketDao& Model::bucketDao()
{
    return *m_bucketDao;
}

dao::AbstractStorageDao& Model::storageDao()
{
    return *m_storageDao;
}

} // namespace nx::cloud::storage::service::model
