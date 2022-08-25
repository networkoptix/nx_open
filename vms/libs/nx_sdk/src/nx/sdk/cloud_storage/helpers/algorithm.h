// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "data.h"

namespace nx {
namespace sdk {
namespace cloud_storage {

bool bookmarkMatches(const Bookmark& bookmark, const BookmarkFilter& filter);
void sortAndLimitBookmarks(const BookmarkFilter& filter, std::vector<Bookmark>* outBookmarks);

bool motionMaches(const Motion& motion, const MotionFilter& filter);
TimePeriodList sortAndLimitMotion(const MotionFilter& filter, TimePeriodList motion);

bool objectTrackMatches(const ObjectTrack& objectTrack, const AnalyticsFilter& filter);
void sortAndLimitTimePeriods(
    SortOrder order, std::optional<int> limit, TimePeriodList* outTimePeriods);

std::vector<uint8_t> fromBase64(const std::string& data);
std::string toBase64(const std::vector<uint8_t>& data);
std::string toBase64(const uint8_t* data, int size);

nx::sdk::cloud_storage::IDeviceAgent* findDeviceAgentById(
    const std::string& id,
    const std::vector<nx::sdk::Ptr<nx::sdk::cloud_storage::IDeviceAgent>>& devices);

template<typename T>
std::string dumpObjects(const std::vector<T>& objects)
{
    std::vector<nx::kit::Json> jsons;
    for (const auto& entry: objects)
        jsons.push_back(nx::kit::Json(entry));

    return nx::kit::Json(jsons).dump();
}

} // namespace cloud_storage
} // namespace sdk
} // namespace nx
