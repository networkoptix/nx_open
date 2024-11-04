// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_spectrum_widget.h"

#include <QtCore/QElapsedTimer>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsLinearLayout>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <nx/vms/client/core/media/voice_spectrum_analyzer.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <ui/graphics/items/resource/voice_spectrum_painter.h>
#include <ui/workaround/sharp_pixmap_painting.h>

namespace nx::vms::client::desktop {

constexpr QRect kLeftGeometry(0, 0, 48 * 3, 48 * 3);

// Moving the icon a bit to the right here, otherwise it looks off-center because of the
// half-circle to the left.
constexpr QMargins kLeftInternalMargins(48 + 6, 48, 48 - 6, 48);

constexpr int kLeftRightSpacing = 2;

constexpr QRect kRightGeometry(kLeftGeometry.width() + kLeftRightSpacing, 0,
                               kLeftGeometry.width() * 3, kLeftGeometry.height());

constexpr QMargins kRightInternalMargins(48, 24, 48, 24);

constexpr QSize kWidgetSize(kLeftGeometry.width() + kLeftRightSpacing + kRightGeometry.width(),
                            kLeftGeometry.height());

//-------------------------------------------------------------------------------------------------
// AudioSpectrumWidget::Private

struct AudioSpectrumWidget::Private
{
    QnResourceDisplayPtr display;
    QIcon icon;
    QPixmap unmutedPixmap;
    QPixmap mutedPixmap;
    QColor backgroundColor;
    QColor activeColor;
    QColor inactiveColor;
    VoiceSpectrumPainter painter;
    QElapsedTimer timer;
    bool muted = false;
};

//-------------------------------------------------------------------------------------------------
// AudioSpectrumWidget

AudioSpectrumWidget::AudioSpectrumWidget(
    QnResourceDisplayPtr display,
    QGraphicsWidget* parent)
    :
    base_type(parent),
    d(new Private{
        .display = display,
        .icon = qnSkin->icon("item/audio_on.svg", "item/audio_off.svg"),
        .backgroundColor = colorTheme()->color("camera.audioOnly.background"),
        .activeColor = colorTheme()->color("camera.audioOnly.visualizer.active"),
        .inactiveColor = colorTheme()->color("camera.audioOnly.visualizer.inactive")
    })
{
    NX_ASSERT(display);

    // Init pixmaps.
    QSize pixmapSize = core::Geometry::eroded(kLeftGeometry, kLeftInternalMargins).size();
    d->unmutedPixmap = d->icon.pixmap(pixmapSize, QIcon::Normal, QIcon::On);
    d->mutedPixmap = d->icon.pixmap(pixmapSize, QIcon::Normal, QIcon::Off);

    // Init layout.
    setMaximumSize(kWidgetSize);
    setMinimumSize(kWidgetSize);

    // Set up event handling.
    setAcceptedMouseButtons(Qt::NoButton);

    // Set up visualizer.
    d->timer.start();
}

AudioSpectrumWidget::~AudioSpectrumWidget()
{
}

bool AudioSpectrumWidget::isMuted() const {
    return d->muted;
}

void AudioSpectrumWidget::setMuted(bool muted) {
    d->muted = muted;
}

void AudioSpectrumWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    // Prepare for arc drawing.
    QRectF leftArcRect = kLeftGeometry;
    QRectF rightArcRect = core::Geometry::aligned(kLeftGeometry.size(), kRightGeometry,
                                                  Qt::AlignRight);

    // Draw icon background.
    QRectF leftRect = kLeftGeometry;
    QPainterPath path;
    path.moveTo((leftArcRect.topLeft() + leftArcRect.topRight()) / 2);
    path.lineTo(leftRect.topRight());
    path.lineTo(leftRect.bottomRight());
    path.lineTo((leftArcRect.bottomLeft() + leftArcRect.bottomLeft()) / 2);
    path.arcTo(leftArcRect, 270, -180);
    painter->fillPath(path, d->backgroundColor);

    // Draw icon.
    QRectF iconRect = core::Geometry::eroded(kLeftGeometry, kLeftInternalMargins);
    paintPixmapSharp(painter, d->muted ? d->mutedPixmap : d->unmutedPixmap, iconRect);

    // Draw spectrum background.
    QRectF rightRect = kRightGeometry;
    path.clear();
    path.moveTo((rightArcRect.bottomLeft() + rightArcRect.bottomRight()) / 2);
    path.lineTo(rightRect.bottomLeft());
    path.lineTo(rightRect.topLeft());
    path.lineTo((rightArcRect.topLeft() + rightArcRect.topRight()) / 2);
    path.arcTo(rightArcRect, 90, -180);
    painter->fillPath(path, d->backgroundColor);

    // Draw spectrum.
    QRectF spectrumRect = core::Geometry::eroded(rightRect, kRightInternalMargins);
    d->painter.update(d->timer.elapsed(), d->display->camDisplay()->audioSpectrum().data);
    d->painter.options.visualizerLineOffset = spectrumRect.width() / 30;
    d->painter.options.color = isMuted() ? d->inactiveColor : d->activeColor;
    d->painter.paint(painter, spectrumRect);
}

} // namespace nx::vms::client::desktop
