#include "rdb_model.h"

#include "temporary_credentials_dao.h"

namespace nx::cloud::db::dao::rdb {

Model createModel()
{
    Model model;
    model.temporaryCredentialsDao = std::make_unique<TemporaryCredentialsDao>();
    return model;
}

} // namespace nx::cloud::db::dao::rdb
