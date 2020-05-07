#pragma once

#include <optional>

namespace nx::analytics::db {

class NX_ANALYTICS_DB_API AbstractObjectTypeDictionary
{
public:
    virtual ~AbstractObjectTypeDictionary() = default;

    /**
     * NOTE: This function is not expected to block.
     * Doing so may degrade analytics DB performance significantly.
     */
    virtual std::optional<QString> idToName(const QString& id) const = 0;
};

} // namespace nx::analytics::db
