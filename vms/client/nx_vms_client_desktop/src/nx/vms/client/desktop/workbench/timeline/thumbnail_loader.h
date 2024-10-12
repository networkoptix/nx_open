// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <QtCore/QHash>
#include <QtCore/QSharedPointer>
#include <QtCore/QSize>
#include <QtGui/QImage>

#include <core/resource/resource_fwd.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/utils/impl_ptr.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <utils/common/aspect_ratio.h>

#include "archive_frame_extractor.h"
#include "thumbnail.h"

namespace nx::vms::client::desktop::workbench::timeline {

class ThumbnailLoader: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum class Mode
    {
        timeLinePanel,
        livePreviewTooltip,
    };

    struct TransformParams
    {
        QSize size;
        double rotation = 0.0;
    };

    ThumbnailLoader(const QnMediaResourcePtr& resource, Mode mode);
    virtual ~ThumbnailLoader() override;

    std::chrono::milliseconds timeStep() const;
    void setTimeStep(std::chrono::milliseconds duration);

    TransformParams transformParams() const;
    void setTransformParams(const TransformParams& params);

    QnTimePeriod timeWindow() const;
    void setTimeWindow(const QnTimePeriod& newTimeWindow);

    QVector<ThumbnailPtr> thumbnails() const;

    ThumbnailPtr thumbnailAt(
        std::chrono::milliseconds desiredTime,
        std::chrono::milliseconds* thumbnailTime = nullptr);

    void setRecordedTimePeriods(const QnTimePeriodList& recordedTimePeriods);

signals:
    void thumbnailLoaded(nx::vms::client::desktop::workbench::timeline::ThumbnailPtr thumbnail);
    void thumbnailsInvalidated();
    void scalingTargetSizeChanged(QSize targetSize);

private:
    void invalidateThumbnails();
    void process();
    void addThumbnail(const ThumbnailPtr& thumbnail);
    void handleExtractedFrame(ArchiveFrameExtractor::Result request);

private:
    const Mode m_mode;
    TransformParams m_transformParams;

    std::unique_ptr<ArchiveFrameExtractor> m_frameExtractor;
    std::optional<QnTimePeriodList> m_recordedTimePeriods;

    std::chrono::milliseconds m_timeStep{0};
    QnTimePeriod m_timeWindow;

    QSet<int64_t> m_processedPeriodsIndexes;
    QHash<int64_t, ThumbnailPtr> m_thumbnailsByPeriodIndex;

    int m_generation = 0;
};

} // namespace nx::vms::client::desktop::workbench::timeline
