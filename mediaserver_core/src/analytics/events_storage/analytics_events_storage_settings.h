#pragma once

#include <nx/utils/db/types.h>

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

class Settings
{
public:
    nx::utils::db::ConnectionOptions dbConnectionOptions;
};

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
