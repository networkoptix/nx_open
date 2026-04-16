// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_data.h"

#include <core/resource/resource.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/text/human_readable.h>

using nx::vms::text::HumanReadable;

namespace nx::vms::client::mobile {
namespace timeline {
namespace {

qint64 getPreviewTimeMs(const QnTimePeriod& motion)
{
    return motion.startTimeMs + motion.durationMs / 2;
}

} // namespace

MotionData::MotionData(const QnTimePeriod& motion, const QnResourcePtr& resource):
    m_motion(motion),
    m_imagePath(makeImageRequest(resource, getPreviewTimeMs(motion), kHighImageResolution)),
    m_resource(resource)
{
}

nx::Uuid MotionData::id() const
{
    return {};
}

qint64 MotionData::startTimeMs() const
{
    return m_motion.startTimeMs;
}

qint64 MotionData::durationMs() const
{
    return m_motion.durationMs;
}

QString MotionData::title() const
{
    return tr("Motion detected");
}

QString MotionData::description() const
{
    return {};
}

QString MotionData::imagePath() const
{
    return m_imagePath;
}

QVariant MotionData::tags() const
{
    return QVariantList{};
}

QVariant MotionData::attributes() const
{
    return QVariantList{};
}

QnResourcePtr MotionData::resource() const
{
    return m_resource;
}

MultiObjectData MotionData::merge(
    std::span<const QnTimePeriod> motions,
    int maxMotionsPerBucket,
    const QnResourcePtr& resource)
{
    if (motions.size() == 1)
    {
        const auto durationText = HumanReadable::timeSpan(motions.front().duration(),
            HumanReadable::Hours | HumanReadable::Minutes | HumanReadable::Seconds,
            HumanReadable::SuffixFormat::Short, " ");

        const auto previewTimeMs = getPreviewTimeMs(motions.back());

        const auto perObjectData = std::make_shared<MotionData>(motions.front(), resource);

        return MultiObjectData{
            .caption = tr("Motion detected"),
            .description = durationText,
            .iconPaths = {"image://skin/20x20/Outline/motion.svg"},
            .imagePaths = {makeImageRequest(resource, previewTimeMs, kLowImageResolution)},
            .positionMs = motions.front().startTimeMs,
            .durationMs = motions.front().durationMs,
            .count = 1,
            .perObjectData = {perObjectData}};
    }
    else if (motions.size() > 1)
    {
        QStringList imagePaths;
        for (auto it = std::prev(motions.end());; --it)
        {
            imagePaths.push_back(
                makeImageRequest(resource, getPreviewTimeMs(*it), kLowImageResolution));
            if (it == motions.begin() || imagePaths.size() >= kMaxPreviewImageCount)
                break;
        }

        return MultiObjectData{
            .caption = (int) motions.size() > maxMotionsPerBucket
                ? tr("Motion (>%1)").arg(maxMotionsPerBucket)
                : tr("Motion (%1)").arg(motions.size()),
            .iconPaths = {"image://skin/20x20/Outline/motion.svg"},
            .imagePaths = imagePaths,
            .positionMs = motions.front().startTimeMs,
            .durationMs = motions.back().startTimeMs - motions.front().startTimeMs,
            .count = (int) motions.size()};
    }
    return {};
}

} // namespace timeline
} // namespace nx::vms::client::mobile
