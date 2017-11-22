#pragma once

#include <nx/utils/db/types.h>

class QnSettings;

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

class Settings
{
public:
    nx::utils::db::ConnectionOptions dbConnectionOptions;

    void load(const QnSettings& settings);
};

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
