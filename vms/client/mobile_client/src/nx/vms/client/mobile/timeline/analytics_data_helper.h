// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <span>
#include <tuple>

#include <QtCore/QCoreApplication>

#include <analytics/db/analytics_db_types.h>

#include "object_data.h"

namespace nx::analytics::taxonomy { class AbstractState; }

namespace nx::vms::client::mobile {
namespace timeline {

class AnalyticsDataHelper
{
    Q_DECLARE_TR_FUNCTIONS(nx::vms::client::mobile::timeline::AnalyticsDataHelper)

public:
    static ObjectData makeObjectData(
        const analytics::db::ObjectTrack& track, const QnResourcePtr& resource);

    static QString makeImageRequest(
        const QnResourcePtr& resource, const analytics::db::ObjectTrack& track, int resolution);

    /** Returns up to `limit` distinct object type names and icon paths of `tracks`. */
    static std::tuple<QStringList /*names*/, QStringList /*iconPaths*/> objectTypeInfos(
        const std::shared_ptr<nx::analytics::taxonomy::AbstractState>& taxonomy,
        std::span<const analytics::db::ObjectTrackEx> tracks,
        int limit);
};

} // namespace timeline
} // namespace nx::vms::client::mobile
