#include "model.h"

#include "nx/cloud/storage/service/settings.h"
#include "database.h"

namespace nx::cloud::storage::service::model {

Model::Model(const conf::Settings& settings):
    m_database(std::make_unique<Database>(settings))
{
}

Model::~Model() = default;

void Model::stop()
{
    m_database->stop();
}

Database* Model::database()
{
    return m_database.get();
}

} // namespace nx::cloud::storage::service::model
