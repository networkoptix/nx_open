#include "update_request_data_factory.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <utils/common/app_info.h>

namespace nx {
namespace update {
namespace detail {

UpdateFileRequestDataFactory::FactoryFunc UpdateFileRequestDataFactory::s_factoryFunc = nullptr;

update::info::UpdateFileRequestData UpdateFileRequestDataFactory::create()
{
    if (s_factoryFunc)
        return s_factoryFunc();

    return update::info::UpdateFileRequestData(
        nx::network::SocketGlobals::cloud().cloudHost(),
        QnAppInfo::customizationName(),
        QnSoftwareVersion(QnAppInfo::applicationVersion()),
        update::info::OsVersion(
            QnAppInfo::applicationPlatform(),
            QnAppInfo::applicationArch(),
            QnAppInfo::applicationPlatformModification()));
}

void UpdateFileRequestDataFactory::setFactoryFunc(UpdateFileRequestDataFactory::FactoryFunc factoryFunc)
{
    s_factoryFunc = std::move(factoryFunc);
}

} // namespace detail
} // namespace update
} // namespace nx
