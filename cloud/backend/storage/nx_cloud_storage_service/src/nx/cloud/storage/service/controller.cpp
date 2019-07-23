#include "controller.h"

#include "model.h"
#include "settings.h"
#include "bucket/manager.h"
#include "storage/manager.h"

namespace nx::cloud::storage::service {

Controller::Controller(const conf::Settings& settings, Model* model):
    m_bucketManager(bucket::ManagerFactory::instance().create(settings, model->database())),
    m_storageManager(storage::ManagerFactory::instance().create(settings, model->database()))
{
}

Controller::~Controller() = default;

bucket::AbstractManager* Controller::bucketManager()
{
    return m_bucketManager.get();
}

storage::AbstractManager* Controller::storageManager()
{
    return m_storageManager.get();
}

} // namespace nx::cloud::storage::service