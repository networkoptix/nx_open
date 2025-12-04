// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nx/reflect/instrument.h>
#include <nx/utils/url_query.h>

namespace nx::cloud::db::api {

/**
    * The Motion object passed to the plugin when bookmark is saved. And the same object should be
    * returned to the Server when queried back if it matches the filter. Plugin should keep motion as
    * is, without any changes.
    */
struct Motion
{
    std::string organizationId;
    std::string siteId;
    std::string deviceId;
    std::chrono::milliseconds startTimeMs;
    std::chrono::milliseconds durationMs;
    int channel = 0;

    /**
        * Binary motion mask data.
        *
        * This mask covers the frame as a 44x32 cells grid. Every non zero bit in the mask means that
        * motion was detected in that cell. So, the bit mask size is 44 * 32 = 1408 bits = 176 bytes
        * before encoding to base64. The mask is rotated by 90 degree. The very first bit of the mask
        * is the top-left corner bit. The next bit is for 1st column, 2nd row, etc.
        */
    std::vector<uint8_t> region;
};

NX_REFLECTION_INSTRUMENT(Motion,
    (organizationId)(siteId)(deviceId)(startTimeMs)(durationMs)(channel)(region))
NX_REFLECTION_TAG_TYPE(Motion, jsonSerializeChronoDurationAsNumber)

NX_REFLECTION_ENUM_CLASS(SortOrder,
    ascending,
    descending
);

struct GetMotionParamsData
{
    std::string organizationId;
    std::string siteId;

    std::vector<std::string> deviceIds;
    std::optional<std::chrono::milliseconds> startTimeMs;      //< UTC datetime.
    std::optional<std::chrono::milliseconds> endTimeMs;    //< UTC datetime.

    /**
     * Regions of screen which should be matched against the motion data.
     *
     * Motion matches if motion.data intersected with any of rectangles in regions contains at
     * least one cell of motion i.e. the corresponding bit in the motion.data is set to 1.
     */
    std::vector<std::string> boundingBox;  //< e.g. "{x},{y},{width}x{height}".

    /** Maximum number of objects to return. */
    std::optional<int> limit;

    /** Found periods are sorted by the start timestamp using this order. */
    SortOrder order = SortOrder::ascending;

    /**
     * If distance between two time periods less than this value, then those periods must be merged
     * ignoring the gap.
     */
    std::optional<std::chrono::milliseconds> interval;

    nx::UrlQuery toUrlQuery() const;
};

} // namespace nx::cloud::db::api
