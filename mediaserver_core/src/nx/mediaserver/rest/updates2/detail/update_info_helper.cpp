
#include <nx/update/info/sync_update_checker.h>
#include "update_info_helper.h"
#include "update_request_data_factory.h"

namespace nx {
namespace mediaserver {
namespace rest {
namespace updates2 {
namespace detail {

bool findUpdateInfo(QnSoftwareVersion* outSoftwareVersion)
{
    const update::info::AbstractUpdateRegistryPtr updateRegistry = update::info::checkSync();
    if (updateRegistry == nullptr)
        return false;

    const auto findResult = updateRegistry->latestUpdate(
        detail::UpdateRequestDataFactory::create(),
        outSoftwareVersion);
    return findResult == update::info::ResultCode::ok;
}

} // namespace detail
} // namespace updates2
} // namespace rest
} // namespace mediaserver
} // namespace nx
