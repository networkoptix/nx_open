#include <nx/network/app_info.h>
#include "update_request_data_factory.h"
#include <utils/common/app_info.h>

namespace nx {
namespace mediaserver {
namespace rest {
namespace updates2 {
namespace detail {

UpdateRequestDataFactory::FactoryFunc UpdateRequestDataFactory::s_factoryFunc = nullptr;

update::info::UpdateRequestData UpdateRequestDataFactory::create()
{
    if (s_factoryFunc)
        return s_factoryFunc();

    return update::info::UpdateRequestData(
        nx::network::AppInfo::defaultCloudHost(),
        QnAppInfo::customizationName(),
        QnSoftwareVersion(QnAppInfo::applicationVersion()));
}

void UpdateRequestDataFactory::setFactoryFunc(UpdateRequestDataFactory::FactoryFunc factoryFunc)
{
    s_factoryFunc = std::move(factoryFunc);
}

} // namespace detail
} // namespace updates2
} // namespace rest
} // namespace mediaserver
} // namespace nx
