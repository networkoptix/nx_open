#include "model.h"

#include "nx/cloud/storage/service/settings.h"
#include "database.h"
#include "dao/bucket_dao.h"
#include "dao/storage_dao.h"

namespace nx::cloud::storage::service::model {

Model::Model(const conf::Settings& settings):
    m_db(std::make_unique<Database>(settings)),
    m_bucketDao(dao::BucketDaoFactory::instance().create(settings.database(), m_db.get())),
    m_storageDao(dao::StorageDaoFactory::instance().create(settings.database(), m_db.get()))
{
}

Model::~Model() = default;

void Model::stop()
{
    NX_INFO(this, "Stopping Model...");
    m_db->stop();
    NX_INFO(this, "Model stopped");
}

Database& Model::database()
{
    return *m_db;
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
