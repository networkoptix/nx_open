// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <QtCore/QHash>

#include <analytics/db/analytics_db_types.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core::detail {

struct AnalyticDataStorage
{
    std::vector<nx::analytics::db::ObjectTrackEx> items; //< Descending sort order.
    QHash<QnUuid, std::chrono::microseconds> idToTimestamp;

    int size() const;

    bool empty() const;

    int indexOf(const QnUuid& trackId) const;

    nx::analytics::db::ObjectTrack take(int index);

    void clear();

    // This function handles single item insertion. It returns index of inserted item.
    // Block insertion is handled outside.
    int insert(nx::analytics::db::ObjectTrack&& item,
        std::function<void(int index)> notifyBeforeInsertion = nullptr,
        std::function<void()> notifyAfterInsertion = nullptr);
};

} // namespace nx::vms::client::core::detail
