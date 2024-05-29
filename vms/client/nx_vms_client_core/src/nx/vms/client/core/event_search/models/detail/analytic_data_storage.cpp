// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytic_data_storage.h"

#include <nx/vms/client/core/event_search/models/analytics_search_list_model.h>

#include "object_track_facade.h"

namespace nx::vms::client::core::detail {

using namespace std::chrono;
using nx::analytics::db::ObjectTrack;

int AnalyticDataStorage::size() const
{
    return (int) items.size();
}

bool AnalyticDataStorage::empty() const
{
    return items.empty();
}

int AnalyticDataStorage::indexOf(const nx::Uuid& trackId) const
{
    const auto timestampIter = idToTimestamp.find(trackId);
    if (timestampIter == idToTimestamp.end())
        return -1;

    auto predicate =
        [](const ObjectTrack& left, std::pair<microseconds, nx::Uuid> right)
        {
            const auto leftTime = ObjectTrackFacade::startTime(left);
            return leftTime > right.first || (leftTime == right.first && left.id > right.second);
        };

    const auto iter = std::lower_bound(
        items.cbegin(), items.cend(),
        std::make_pair(*timestampIter, trackId),
        predicate);

    return iter != items.cend() && iter->id == trackId
        ? (int) std::distance(items.cbegin(), iter)
        : -1;
}

int AnalyticDataStorage::insert(
    nx::analytics::db::ObjectTrack&& item,
    AnalyticsSearchListModel* model)
{
    const auto position = std::upper_bound(items.begin(), items.end(), item,
        [](const ObjectTrack& left, const ObjectTrack& right)
        {
            const auto leftTime = ObjectTrackFacade::startTime(left);
            const auto rightTime = ObjectTrackFacade::startTime(right);
            return leftTime > rightTime || (leftTime == rightTime && left.id > right.id);
        });

    const int index = position - items.begin();
    idToTimestamp[item.id] = ObjectTrackFacade::startTime(item);

    const AnalyticsSearchListModel::ScopedInsertRows insertGuard(model, index, index, !!model);
    items.insert(position, std::move(item));
    return index;
}

nx::analytics::db::ObjectTrack AnalyticDataStorage::take(int index)
{
    nx::analytics::db::ObjectTrack result(std::move(items[index]));
    items.erase(items.begin() + index);
    idToTimestamp.remove(result.id);
    return result;
}

void AnalyticDataStorage::clear()
{
    items.clear();
    idToTimestamp.clear();
}

void AnalyticDataStorage::rebuild()
{
    idToTimestamp.clear();
    for (const auto& item: items)
        idToTimestamp[item.id] = ObjectTrackFacade::startTime(item);
}

} // namespace nx::vms::client::core::detail
