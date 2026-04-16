// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_object_data.h"

namespace nx::vms::client::mobile {
namespace timeline {

class MotionData: public AbstractObjectData
{
    Q_OBJECT
public:
    MotionData(const QnTimePeriod& motion, const QnResourcePtr& resource);

    virtual nx::Uuid id() const override;
    virtual qint64 startTimeMs() const override;
    virtual qint64 durationMs() const override;
    virtual QString title() const override;
    virtual QString description() const override;
    virtual QString imagePath() const override;
    virtual QVariant tags() const override;
    virtual QVariant attributes() const override;
    virtual QnResourcePtr resource() const override;

    static MultiObjectData merge(
        std::span<const QnTimePeriod> motions,
        int maxMotionsPerBucket,
        const QnResourcePtr& resource);

private:
    QnTimePeriod m_motion;
    QString m_imagePath;
    QnResourcePtr m_resource;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
