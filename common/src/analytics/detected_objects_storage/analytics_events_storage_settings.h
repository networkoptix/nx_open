#pragma once

#include <nx/sql/types.h>

class QnSettings;

namespace nx {
namespace analytics {
namespace storage {

class Settings
{
public:
    nx::sql::ConnectionOptions dbConnectionOptions;

    void load(const QnSettings& settings);
};

} // namespace storage
} // namespace analytics
} // namespace nx
