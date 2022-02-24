// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "live_preview.h"

#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>

#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/client/desktop/workbench/timeline/live_preview_thumbnail.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/math/math.h>

using namespace std::chrono;
using namespace nx::vms::client::core;

namespace nx::vms::client::desktop::workbench::timeline {

namespace {

QPointF mapToItemFromGlobal(const QGraphicsItem* item, const QPoint& globalPoint)
{
    const auto scene = item->scene();
    if (!scene)
        return {};

    if (scene->views().isEmpty() || !scene->views().first() || !scene->views().first()->viewport())
        return {};

    const QGraphicsView* v = scene->views().first();
    return item->mapFromScene(v->mapToScene(v->viewport()->mapFromGlobal(globalPoint)));
}

} // namespace

struct LivePreview::Private
{
    LivePreview* const q;
    QnTimeSlider* const slider;
    qreal xPosition = 0;
    milliseconds pointedTime{LivePreviewThumbnail::kNoTimestamp};
    milliseconds lastAlignedTime{LivePreviewThumbnail::kNoTimestamp};
    milliseconds actualPreviewTime{LivePreviewThumbnail::kNoTimestamp};
    bool thumbnailExpected = false;

    bool dataExistsForTime(milliseconds time) const;
    milliseconds timeStep() const;

    void updateLineLength();
    void updateConstraints();

    LivePreviewThumbnail* const thumbnailSource = new LivePreviewThumbnail(q);
    const QmlProperty<AbstractResourceThumbnail*> previewSource{q->widget(), "previewSource"};

    const QmlProperty<qreal> markerLineLength{q->widget(), "markerLineLength"};
    const QmlProperty<qreal> desiredPreviewHeight{q->widget(), "desiredPreviewHeight"};
    const QmlProperty<qreal> maximumPreviewWidth{q->widget(), "maximumPreviewWidth"};
};

LivePreview::LivePreview(QnTimeSlider* slider, QObject* parent):
    base_type(QUrl("Nx/Timeline/private/LivePreview.qml"), slider->context(), parent),
    d(new Private{this, slider})
{
    widget()->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setTimeMarkerDisplay(TimeMarkerDisplay::automatic);

    d->previewSource = d->thumbnailSource;
    d->updateLineLength();

    connect(slider, &QGraphicsWidget::heightChanged, this,
        [this]()
        {
            d->updateLineLength();
            d->updateConstraints();
        });

    connect(slider, &QGraphicsWidget::widthChanged, this,
        [this]() { d->updateConstraints(); });

    connect(slider, &QnTimeSlider::thumbnailsHeightChanged, this,
        [this]() { d->updateConstraints(); });

    connect(this, &BubbleToolTip::stateChanged, this,
        [this](State state)
        {
            if (d->slider)
                d->slider->setThumbnailsFaded(state == State::shown);
        });
}

LivePreview::~LivePreview()
{
    // Required here for forward declared scoped pointer destruction.
}

void LivePreview::setThumbnailLoadingManager(ThumbnailLoadingManager* value)
{
    d->thumbnailSource->setThumbnailLoadingManager(value);
}

void LivePreview::showAt(
    const TimeContent& timeContent, int pointerX, int bottomY, int minX, int maxX)
{
    const auto newResource = navigator()->currentResource();
    if (newResource != d->thumbnailSource->resource())
    {
        d->pointedTime = LivePreviewThumbnail::kNoTimestamp;
        d->lastAlignedTime = LivePreviewThumbnail::kNoTimestamp;
        d->actualPreviewTime = LivePreviewThumbnail::kNoTimestamp;
    }

    d->thumbnailSource->setResource(newResource);
    if (!NX_ASSERT(newResource))
    {
        hide();
        return;
    }

    setTimeContent(timeContent);

    const auto localPos = mapToItemFromGlobal(d->slider, QPoint(pointerX, 0));
    auto newPointedTime = d->slider->timeFromPosition(localPos, /*bound*/ true);

    const auto recordingTimePeriods = d->slider->timePeriods(0, Qn::RecordingContent);
    const auto nearestPeriodItr =
        recordingTimePeriods.findNearestPeriod(newPointedTime.count(), /*searchForward*/ true);

    if (nearestPeriodItr != recordingTimePeriods.cend()
        && !nearestPeriodItr->contains(newPointedTime)
        && (nearestPeriodItr->startTime() - newPointedTime) < d->slider->msecsPerPixel())
    {
        newPointedTime = nearestPeriodItr->startTime();
    }

    d->actualPreviewTime = newPointedTime;
    d->xPosition = pointerX;
    setPosition(pointerX, bottomY, minX, maxX);

    const bool dataExistsForPrevPointedTime = d->dataExistsForTime(d->pointedTime);
    d->pointedTime = newPointedTime;
    const bool dataExistsForNewPointedTime = d->dataExistsForTime(d->pointedTime);
    const bool dataExistsForPointedTimeChanged =
        dataExistsForNewPointedTime != dataExistsForPrevPointedTime;

    const auto alignedTime = milliseconds(qRound(d->pointedTime, d->timeStep().count()));
    const bool newAlignedTime = alignedTime != d->lastAlignedTime;
    d->lastAlignedTime = alignedTime;
    const bool usePerPixelImages =
        !d->dataExistsForTime(d->lastAlignedTime) && dataExistsForNewPointedTime;

    if (newAlignedTime || usePerPixelImages || dataExistsForPointedTimeChanged)
    {
        // Start thumbnail loading via provider.
        d->thumbnailExpected = true;

        if (d->dataExistsForTime(d->pointedTime))
        {
            if (usePerPixelImages)
                d->thumbnailSource->setTimestamp(d->pointedTime);
            else
                d->thumbnailSource->setTimestamp(d->lastAlignedTime);
        }
        else
        {
            d->thumbnailSource->setTimestamp(LivePreviewThumbnail::kNoTimestamp);
        }
    }

    show();
}

bool LivePreview::Private::dataExistsForTime(milliseconds time) const
{
    const auto resource = thumbnailSource->resource();
    if (!NX_ASSERT(resource))
        return false;
    const bool ignorePeriods = resource->hasFlags(Qn::local_video)
        && !resource->hasFlags(Qn::periods);
    return ignorePeriods
        || slider->timePeriods(0, Qn::RecordingContent).containTime(time.count());
}

milliseconds LivePreview::Private::timeStep() const
{
    return slider->msecsPerPixel() * kStepPixels;
}

void LivePreview::Private::updateLineLength()
{
    if (slider)
        markerLineLength = slider->geometry().height();
}

void LivePreview::Private::updateConstraints()
{
    if (!slider)
        return;

    desiredPreviewHeight = slider->thumbnailsHeight();
    maximumPreviewWidth = slider->geometry().width();
}

} // namespace nx::vms::client::desktop::workbench::timeline
