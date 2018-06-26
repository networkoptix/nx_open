#include "update_request_data_factory.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <utils/common/app_info.h>

namespace nx {
namespace update {
namespace manager {
namespace detail {

UpdateRequestDataFactory::FactoryFunc UpdateRequestDataFactory::s_factoryFunc = nullptr;

update::info::UpdateRequestData UpdateRequestDataFactory::create(
    bool isClient,
    const QnSoftwareVersion* targetVersion)
{
    if (s_factoryFunc)
        return s_factoryFunc();

    return update::info::UpdateRequestData(
        nx::network::SocketGlobals::cloud().cloudHost(),
        QnAppInfo::customizationName(),
        QnSoftwareVersion(QnAppInfo::applicationVersion()),
        update::info::OsVersion(
            QnAppInfo::applicationPlatform(),
            QnAppInfo::applicationArch(),
            QnAppInfo::applicationPlatformModification()),
        targetVersion,
        isClient);
}

void UpdateRequestDataFactory::setFactoryFunc(UpdateRequestDataFactory::FactoryFunc factoryFunc)
{
    s_factoryFunc = std::move(factoryFunc);
}

} // namespace detail
} // namespace manager
} // namespace update
} // namespace nx
