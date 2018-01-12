#include <nx/network/app_info.h>
#include "update_request_data_factory.h"
#include <utils/common/app_info.h>

namespace nx {
namespace mediaserver {
namespace updates2 {
namespace detail {

UpdateFileRequestDataFactory::FactoryFunc UpdateFileRequestDataFactory::s_factoryFunc = nullptr;

update::info::UpdateFileRequestData UpdateFileRequestDataFactory::create()
{
    if (s_factoryFunc)
        return s_factoryFunc();

    return update::info::UpdateFileRequestData(
        nx::network::AppInfo::defaultCloudHost(),
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
} // namespace updates2
} // namespace mediaserver
} // namespace nx
