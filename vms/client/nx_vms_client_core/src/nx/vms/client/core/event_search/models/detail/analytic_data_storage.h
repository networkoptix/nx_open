// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <QtCore/QHash>

#include <analytics/db/analytics_db_types.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core { class AnalyticsSearchListModel; }

namespace nx::vms::client::core::detail {

struct AnalyticDataStorage
{
    std::vector<nx::analytics::db::ObjectTrackEx> items; //< Descending sort order.
    QHash<nx::Uuid, std::chrono::microseconds> idToTimestamp;

    int size() const;

    bool empty() const;

    int indexOf(const nx::Uuid& trackId) const;

    nx::analytics::db::ObjectTrack take(int index);

    void clear();

    // This function handles single item insertion. It returns index of inserted item.
    // Block insertion is handled outside.
    int insert(nx::analytics::db::ObjectTrack&& item,
        AnalyticsSearchListModel* model = nullptr);

    void rebuild();
};

} // namespace nx::vms::client::core::detail
