// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_spectrum_widget.h"

#include <QtCore/QElapsedTimer>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsLinearLayout>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/voice_spectrum_painter.h>
#include <utils/media/voice_spectrum_analyzer.h>

namespace nx::vms::client::desktop {

constexpr QSize kButtonSize(48 * 3, 48 * 3);

// Moving the icon a bit to the right here, otherwise it looks off-center because of the half-circle to the left.
constexpr QMargins kButtonInternalMargins(48 + 6, 48, 48 - 6, 48);

constexpr int kButtonSpectrumSpacing = 2;

constexpr QSize kSpectrumSize(kButtonSize.width() * 3, kButtonSize.height());

constexpr QSize kHalfSpectrumSize(kSpectrumSize.width() / 2, kSpectrumSize.height());

constexpr QMargins kSpectrumInternalMargins(48, 24, 48, 24);

//-------------------------------------------------------------------------------------------------
// AudioSpectrumOverlayWidget::Private

struct AudioSpectrumWidget::Private
{
    QnResourceDisplayPtr display;
    QnImageButtonWidget* const button;
    GraphicsWidget* const rightSpacer;
    QColor backgroundColor;
    QColor activeColor;
    QColor inactiveColor;
    VoiceSpectrumPainter painter;
    QElapsedTimer timer;
};

//-------------------------------------------------------------------------------------------------
// AreaSelectOverlayWidget

AudioSpectrumWidget::AudioSpectrumWidget(
    QnResourceDisplayPtr display,
    QGraphicsWidget* parent)
    :
    base_type(parent),
    d(new Private{
        .display = display,
        .button = new QnImageButtonWidget(this),
        .rightSpacer = new GraphicsWidget(this),
        .backgroundColor = colorTheme()->color("camera.audioOnly.background"),
        .activeColor = colorTheme()->color("camera.audioOnly.visualizer.active"),
        .inactiveColor = colorTheme()->color("camera.audioOnly.visualizer.inactive")
    })
{
    NX_ASSERT(display);

    // Init layout.
    QGraphicsLinearLayout* layout = new QGraphicsLinearLayout(Qt::Horizontal);
    layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    layout->addItem(d->button);
    layout->addItem(d->rightSpacer);
    layout->setSpacing(kButtonSpectrumSpacing);
    setLayout(layout);

    // Init button & spacer.
    d->button->setIcon(qnSkin->icon("item/audio_on.svg", "item/audio_off.svg"));
    d->button->setCheckable(true);
    d->button->setFixedSize(kButtonSize);
    d->button->setImageMargins(kButtonInternalMargins);
    d->rightSpacer->setMinimumSize(kSpectrumSize);
    d->rightSpacer->setMaximumSize(kSpectrumSize);

    // Set up event handling.
    setAcceptedMouseButtons(Qt::NoButton);
    d->rightSpacer->setAcceptedMouseButtons(Qt::NoButton);
    d->button->setAcceptedMouseButtons(Qt::NoButton); // It is a fake button.

    // Set up visualizer.
    d->timer.start();
}

AudioSpectrumWidget::~AudioSpectrumWidget()
{
}

bool AudioSpectrumWidget::isMuted() const {
    return d->button->isChecked();
}

void AudioSpectrumWidget::setMuted(bool muted) {
    d->button->setChecked(muted);
}

void AudioSpectrumWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    // Prepare for arc drawing.
    QRectF leftRect = d->button->geometry();
    QRectF rightRect = core::Geometry::aligned(leftRect.size(), d->rightSpacer->geometry(),
                                               Qt::AlignRight);

    // Draw button background.
    QRectF buttonRect = d->button->geometry();
    QPainterPath path;
    path.moveTo((leftRect.topLeft() + leftRect.topRight()) / 2);
    path.lineTo(buttonRect.topRight());
    path.lineTo(buttonRect.bottomRight());
    path.lineTo((leftRect.bottomLeft() + leftRect.bottomLeft()) / 2);
    path.arcTo(leftRect, 270, -180);
    painter->fillPath(path, d->backgroundColor);

    // Draw spectrum background.
    QRectF spacerRect = d->rightSpacer->geometry();
    path.clear();
    path.moveTo((rightRect.bottomLeft() + rightRect.bottomRight()) / 2);
    path.lineTo(spacerRect.bottomLeft());
    path.lineTo(spacerRect.topLeft());
    path.lineTo((rightRect.topLeft() + rightRect.topRight()) / 2);
    path.arcTo(rightRect, 90, -180);
    painter->fillPath(path, d->backgroundColor);

    // Draw spectrum.
    QRectF spectrumRect = core::Geometry::eroded(spacerRect, kSpectrumInternalMargins);
    d->painter.update(d->timer.elapsed(), d->display->camDisplay()->audioSpectrum().data);
    d->painter.options.visualizerLineOffset = spectrumRect.width() / 30;
    d->painter.options.color = isMuted() ? d->inactiveColor : d->activeColor;
    d->painter.paint(painter, spectrumRect);
}

} // namespace nx::vms::client::desktop
