#include "fetch_data.h"

#include <iostream>

#include <nx/cloud/cdb/api/cdb_client.h>

#include <nx/utils/sync_call.h>

namespace nx {
namespace cdb {
namespace client {

int fetchSystems(api::CdbClient* cdbClient)
{
    api::ResultCode resultCode = api::ResultCode::ok;
    api::SystemDataExList systemDataList;
    std::tie(resultCode, systemDataList) =
        makeSyncCall<api::ResultCode, api::SystemDataExList>(
            std::bind(
                &nx::cdb::api::SystemManager::getSystems,
                cdbClient->systemManager(),
                std::placeholders::_1));
    if (resultCode != api::ResultCode::ok)
    {
        std::cerr<<"Failure: "<<api::toString(resultCode)<<std::endl;
        return 1;
    }

    std::cout<<"Received "<< systemDataList.systems.size()<<" systems:"<<std::endl;

    for (const auto& system: systemDataList.systems)
        std::cout<<system.id<<", "<<system.name<<std::endl;

    return 0;
}

int fetchData(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    const std::string& fetchRequest)
{
    api::CdbClient cdbClient;
    cdbClient.setCloudUrl(cdbUrl);
    cdbClient.setCredentials(login, password);

    if (fetchRequest == "systems")
        return fetchSystems(&cdbClient);

    return 0;
}

} // namespace client
} // namespace cdb
} // namespace nx
