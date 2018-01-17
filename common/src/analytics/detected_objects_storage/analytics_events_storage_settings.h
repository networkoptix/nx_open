#pragma once

#include <nx/utils/db/types.h>

class QnSettings;

namespace nx {
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
} // namespace nx
