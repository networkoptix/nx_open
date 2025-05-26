// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "live_preview.h"

#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>

#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/client/desktop/workbench/timeline/live_preview_thumbnail.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/workbench/workbench_navigator.h>

using namespace std::chrono;
using namespace nx::vms::client::core;

namespace nx::vms::client::desktop::workbench::timeline {

struct LivePreview::Private
{
    LivePreview* const q;
    QnTimeSlider* const slider;

    bool dataExistsForTime(milliseconds time) const;
    void setResource(const QnResourcePtr& resource);

    void updateLineLength();
    void updateConstraints();

    LivePreviewThumbnail* const thumbnailSource = new LivePreviewThumbnail(q);
    const QmlProperty<AbstractResourceThumbnail*> previewSource{q->widget(), "previewSource"};

    const QmlProperty<qreal> markerLineLength{q->widget(), "markerLineLength"};
    const QmlProperty<qreal> desiredPreviewHeight{q->widget(), "desiredPreviewHeight"};
    const QmlProperty<qreal> maximumPreviewWidth{q->widget(), "maximumPreviewWidth"};

    nx::utils::ScopedConnections resourceConnections;
};

LivePreview::LivePreview(QnTimeSlider* slider, QObject* parent):
    base_type(QUrl("Nx/Timeline/private/LivePreview.qml"), slider->windowContext(), parent),
    d(new Private{this, slider})
{
    if (ini().debugDisableQmlTooltips)
        return;

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

            if (state != State::shown)
                d->setResource({});
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
    const QPoint& globalCursorPos,
    const QRectF& globalTimeSliderRect,
    const TimeContent& timeContent)
{
    const auto newResource = navigator()->currentResource();
    d->setResource(newResource);

    if (!NX_ASSERT(newResource))
    {
        hide();
        return;
    }

    setPosition(
        globalCursorPos.x(),
        globalTimeSliderRect.top(),
        globalTimeSliderRect.left(),
        globalTimeSliderRect.right());

    setTimeContent(timeContent);

    d->thumbnailSource->setTimestamp(d->dataExistsForTime(timeContent.position)
        ? timeContent.position
        : LivePreviewThumbnail::kNoTimestamp);

    show();
}

bool LivePreview::Private::dataExistsForTime(milliseconds timePoint) const
{
    const auto resource = thumbnailSource->resource();
    if (!NX_ASSERT(resource))
        return false;

    if (resource->hasFlags(Qn::local_video) && !resource->hasFlags(Qn::periods))
        return true;

    const QnTimePeriod testTimePeriod(std::max(0ms, timePoint - slider->msecsPerPixel() / 2),
        slider->msecsPerPixel());

    return slider->timePeriods(0, Qn::RecordingContent).intersects(testTimePeriod);
}

void LivePreview::Private::setResource(const QnResourcePtr& resource)
{
    if (thumbnailSource->resource() == resource)
        return;

    NX_VERBOSE(q, "Setting resource to %1", resource);

    resourceConnections.reset();
    thumbnailSource->setResource(resource);

    if (!resource)
        return;

    resourceConnections << QObject::connect(resource.get(), &QnResource::flagsChanged, q,
        [this](const QnResourcePtr& resource)
        {
            if (resource == thumbnailSource->resource() && resource->hasFlags(Qn::removed))
                setResource({});
        });
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
