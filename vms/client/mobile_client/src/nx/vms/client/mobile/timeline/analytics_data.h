// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/db/analytics_db_types.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>

#include "abstract_object_data.h"

namespace nx::vms::client::mobile {
namespace timeline {

class AnalyticsData: public AbstractObjectData
{
    Q_OBJECT
public:
    AnalyticsData(const analytics::db::ObjectTrack& track, const QnResourcePtr& resource);

    virtual nx::Uuid id() const override;
    virtual qint64 startTimeMs() const override;
    virtual qint64 durationMs() const override;
    virtual QString title() const override;
    virtual QString description() const override;
    virtual QString imagePath() const override;
    virtual QVariant tags() const override;
    virtual QVariant attributes() const override;
    virtual QnResourcePtr resource() const override;

    void update(analytics::db::ObjectTrack track);

    static MultiObjectData merge(
        std::span<const analytics::db::ObjectTrackEx> tracks,
        const QnTimePeriod& period,
        std::chrono::milliseconds minimumStackDuration,
        int limit,
        const QnResourcePtr& resource);

private:
    analytics::db::ObjectTrack m_track;
    QString m_title;
    QString m_imagePath;
    core::analytics::AttributeList m_attributes;
    QnResourcePtr m_resource;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
