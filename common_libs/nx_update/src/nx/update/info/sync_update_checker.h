#pragma once

#include <nx/update/info/async_update_checker.h>

namespace nx {
namespace update {
namespace info {

NX_UPDATE_API AbstractUpdateRegistryPtr checkSync(const QString& url = kDefaultUrl);

} // namespace info
} // namespace update
} // namespace nx
