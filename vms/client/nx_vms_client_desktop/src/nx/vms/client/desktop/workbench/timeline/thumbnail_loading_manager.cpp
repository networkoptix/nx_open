// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_loading_manager.h"

#include <utils/math/math.h>
#include <nx/utils/log/assert.h>
#include <recording/time_period_list.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include "live_preview.h"
#include "thumbnail_loader.h"

using namespace std::chrono;

namespace {

QSize calculateThumbnailSize(
    int sizeConstraint,
    Qt::Orientation constraintOrientation,
    QnAspectRatio aspectRatio,
    double rotation)
{
    if (!aspectRatio.isValid())
        return QSize();

    const bool transpose = QnAspectRatio::isRotated90(rotation);
    const bool isHorizontalConstraint =
        (constraintOrientation == Qt::Horizontal) != transpose;

    const auto size = isHorizontalConstraint
        ? QSize(sizeConstraint, qRound(sizeConstraint / aspectRatio.toFloat()))
        : QSize(qRound(aspectRatio.toFloat() * sizeConstraint), sizeConstraint);

    return transpose ? size.transposed() : size;
}

double roundRotationToQuadrant(double rotation)
{
    return qRound(rotation, 90.0);
}

} // namespace

namespace nx::vms::client::desktop::workbench::timeline {

namespace {

static constexpr int kLivePreviewTooltipWidth = 128;
static const QnAspectRatio kDefaultAspectRatio = {16, 9};

} // namespace

ThumbnailLoadingManager::ThumbnailLoadingManager(
    QnMediaResourceWidget* mediaResourceWidget,
    QnTimeSlider* slider)
    :
    m_mediaResourceWidget(mediaResourceWidget),
    m_slider(slider),
    m_panelLoader(new ThumbnailLoader(
        m_mediaResourceWidget->resource(), ThumbnailLoader::Mode::timeLinePanel)),
    m_livePreviewLoader(new ThumbnailLoader(
        m_mediaResourceWidget->resource(), ThumbnailLoader::Mode::livePreviewTooltip)),
    m_rotation(roundRotationToQuadrant(mediaResourceWidget->rotation())),
    m_panelVisible(m_slider->isThumbnailsVisible())
{
    connect(m_panelLoader.get(), &ThumbnailLoader::thumbnailLoaded,
        this, &ThumbnailLoadingManager::panelThumbnailLoaded);

    connect(m_panelLoader.get(), &ThumbnailLoader::thumbnailsInvalidated,
        this, &ThumbnailLoadingManager::panelThumbnailsInvalidated);

    connect(m_livePreviewLoader.get(), &ThumbnailLoader::thumbnailLoaded,
        this, &ThumbnailLoadingManager::individualPreviewThumbnailLoaded);

    connect(m_livePreviewLoader.get(), &ThumbnailLoader::scalingTargetSizeChanged,
        this, &ThumbnailLoadingManager::previewTargetSizeChanged);

    connect(m_mediaResourceWidget, &QnMediaResourceWidget::aspectRatioChanged,
        this, &ThumbnailLoadingManager::aspectRatioChanged);

    connect(m_mediaResourceWidget, &QnMediaResourceWidget::rotationChanged, this,
        [this]
        {
            const auto newRotation = roundRotationToQuadrant(m_mediaResourceWidget->rotation());
            if (qFuzzyEquals(m_rotation, newRotation))
                return;

            m_rotation = newRotation;
            emit rotationChanged();
        });
}

ThumbnailLoadingManager::~ThumbnailLoadingManager()
{
}

QnMediaResourceWidget* ThumbnailLoadingManager::resourceWidget() const
{
    return m_mediaResourceWidget;
}

void ThumbnailLoadingManager::refresh(
    int boundingHeight,
    milliseconds timeStep,
    const QnTimePeriod& panelWindowPeriod)
{
    m_panelBoundingHeight = boundingHeight;
    m_panelWindowPeriod = panelWindowPeriod;
    m_panelVisible = m_slider->isThumbnailsVisible();

    ThumbnailLoader::TransformParams panelTransformParams;
    panelTransformParams.size = QnAspectRatio::isRotated90(rotation())
        ? panelThumbnailSize().transposed()
        : panelThumbnailSize();
    panelTransformParams.rotation = rotation();

    ThumbnailLoader::TransformParams tooltipTransformParams;
    tooltipTransformParams.size = QnAspectRatio::isRotated90(rotation())
        ? previewThumbnailSize().transposed()
        : previewThumbnailSize();

    tooltipTransformParams.rotation = m_livePreviewPreRotated ? rotation() : 0;

    // Set target thumbnail size for loaders.
    if (m_slider->isThumbnailsVisible())
        m_panelLoader->setTransformParams(panelTransformParams);

    m_livePreviewLoader->setTransformParams(tooltipTransformParams);

    // Set time step for loaders.
    if (m_slider->isThumbnailsVisible())
        m_panelLoader->setTimeStep(timeStep);

    static constexpr auto kLivePreviewTimeStepUpdateThresholdRatio = 1.1;
    static constexpr auto kMinimumLivePreviewTimeStep = 40ms;

    const auto livePreviewTimeStep = m_livePreviewLoader->timeStep();
    const auto newLivePreviewTimeStep = std::max(
        kMinimumLivePreviewTimeStep,
        m_slider->msecsPerPixel() * LivePreview::kStepPixels);

    if (livePreviewTimeStep != 0ms && newLivePreviewTimeStep != 0ms)
    {
        const auto livePreviewTimeStepChangeRatio =
            std::max<double>(livePreviewTimeStep.count(), newLivePreviewTimeStep.count())
            / std::min<double>(livePreviewTimeStep.count(), newLivePreviewTimeStep.count());

        if (livePreviewTimeStepChangeRatio > kLivePreviewTimeStepUpdateThresholdRatio)
            m_livePreviewLoader->setTimeStep(newLivePreviewTimeStep);
    }
    else
    {
        m_livePreviewLoader->setTimeStep(newLivePreviewTimeStep);
    }

    applyPanelWindowPeriod();
}

QnAspectRatio ThumbnailLoadingManager::aspectRatio() const
{
    if (m_mediaResourceWidget->hasAspectRatio())
        return QnAspectRatio::closestStandardRatio(m_mediaResourceWidget->aspectRatio());

    return kDefaultAspectRatio;
}

int ThumbnailLoadingManager::panelBoundingHeight() const
{
    return m_panelBoundingHeight;
}

QSize ThumbnailLoadingManager::panelThumbnailSize() const
{
    return calculateThumbnailSize(m_panelBoundingHeight, Qt::Vertical, aspectRatio(), rotation());
}

milliseconds ThumbnailLoadingManager::panelTimeStep() const
{
    return m_panelLoader->timeStep();
}

qreal ThumbnailLoadingManager::rotation() const
{
    return m_rotation;
}

QSize ThumbnailLoadingManager::previewThumbnailSize() const
{
    if (m_panelVisible)
        return panelThumbnailSize();

    return calculateThumbnailSize(
        kLivePreviewTooltipWidth, Qt::Horizontal, aspectRatio(), rotation());
}

milliseconds ThumbnailLoadingManager::previewTimeStep() const
{
    return m_livePreviewLoader->timeStep();
}

bool ThumbnailLoadingManager::wasPanelVisibleAtLastRefresh() const
{
    return m_panelVisible;
}

void ThumbnailLoadingManager::setPanelWindowPeriod(const QnTimePeriod& panelWindowPeriod)
{
    if (panelWindowPeriod == m_panelWindowPeriod)
        return;

    m_panelWindowPeriod = panelWindowPeriod;
    applyPanelWindowPeriod();
}

void ThumbnailLoadingManager::applyPanelWindowPeriod()
{
    if (m_slider->isThumbnailsVisible())
        m_panelLoader->setTimeWindow(m_panelWindowPeriod);
}

QVector<ThumbnailPtr> ThumbnailLoadingManager::panelThumbnails() const
{
    return m_panelLoader->thumbnails();
}

ThumbnailPtr ThumbnailLoadingManager::previewThumbnailAt(
    milliseconds desiredTime,
    milliseconds* outThumbnailTime)
{
    return m_livePreviewLoader->thumbnailAt(desiredTime, outThumbnailTime);
}

void ThumbnailLoadingManager::setRecordedTimePeriods(const QnTimePeriodList& recordedTimePeriods)
{
    m_panelLoader->setRecordedTimePeriods(recordedTimePeriods);
    m_livePreviewLoader->setRecordedTimePeriods(recordedTimePeriods);
}

bool ThumbnailLoadingManager::livePreviewPreRotated() const
{
    return m_livePreviewPreRotated;
}

void ThumbnailLoadingManager::setLivePreviewPreRotated(bool value)
{
    m_livePreviewPreRotated = value;
}

} // nx::vms::client::desktop::workbench::timeline
