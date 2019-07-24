#include "manager.h"

#include "../settings.h"
#include "../database.h"

namespace nx::cloud::storage::service::storage {

using namespace std::placeholders;

Manager::Manager(Database* /*database*/)/*:
    m_database(database)*/
{
}

void Manager::addStorage(
    const api::AddStorageRequest& request,
    nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler)
{
    api::Storage response;
    response.totalSpace = request.totalSpace;
    response.freeSpace = request.totalSpace;
    handler(api::Result{api::ResultCode::ok, "addOk"}, std::move(response));
}

void Manager::readStorage(
    const std::string& storageId,
    nx::utils::MoveOnlyFunc<void(api::Result, api::Storage)> handler)
{
    api::Storage response;
    response.id = storageId;
    handler(api::Result{api::ResultCode::ok, "readOk"}, std::move(response));
}

void Manager::removeStorage(
    const std::string& /*storageId*/,
    nx::utils::MoveOnlyFunc<void(api::Result)> handler)
{
    handler(api::Result{api::ResultCode::ok, "removeOk"});
}

void Manager::listCameras(
    const std::string& /*storageId*/,
    nx::utils::MoveOnlyFunc<void(api::Result, std::vector<std::string>)> handler)
{
    handler(api::Result{api::ResultCode::ok, "listCamerasOk"}, std::vector<std::string>());
}

ManagerFactory::ManagerFactory():
    base_type(std::bind(&ManagerFactory::defaultFactoryFunction, this, _1, _2))
{
}

ManagerFactory& ManagerFactory::instance()
{
    static ManagerFactory factory;
    return factory;
}

std::unique_ptr<AbstractManager> ManagerFactory::defaultFactoryFunction(
    const  conf::Settings& /*settings*/, Database* database)
{
    return std::make_unique<Manager>(database);
}

} // namespace nx::cloud::storage::service:: storage