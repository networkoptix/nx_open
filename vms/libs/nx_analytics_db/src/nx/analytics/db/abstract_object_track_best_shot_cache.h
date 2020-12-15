#pragma once

#include <optional>

#include <nx/utils/uuid.h>
#include <analytics/common/object_metadata.h>
#include <analytics/db/analytics_db_types.h>

namespace nx::analytics::db {

/**
 * Allows to store and fetch Object Track Best Shots.
 */
class AbstractObjectTrackBestShotCache
{

public:
    virtual ~AbstractObjectTrackBestShotCache() {}

    virtual void promiseBestShot(QnUuid trackId) = 0;

    virtual void insert(QnUuid trackId, Image image) = 0;

    virtual std::optional<Image> fetch(QnUuid trackId) const = 0;
};

} // namespace nx::analytics::db
