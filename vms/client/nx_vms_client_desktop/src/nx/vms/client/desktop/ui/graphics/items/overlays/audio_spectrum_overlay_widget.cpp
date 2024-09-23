// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_spectrum_overlay_widget.h"

#include <QtWidgets/QGraphicsGridLayout>

#include <camera/cam_display.h>
#include <camera/resource_display.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include "audio_spectrum_widget.h"

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// AudioSpectrumOverlayWidget::Private

struct AudioSpectrumOverlayWidget::Private
{
    QnResourceDisplayPtr const display;
    AudioSpectrumWidget* const widget;
};

//-------------------------------------------------------------------------------------------------
// AudioSpectrumOverlayWidget

AudioSpectrumOverlayWidget::AudioSpectrumOverlayWidget(
    QnResourceDisplayPtr display,
    QGraphicsWidget* parent)
    :
    base_type(parent),
    d(new Private{
        .display = display,
        .widget = new AudioSpectrumWidget(display, this)
    })
{
    NX_ASSERT(display);

    // Don't interfere with event handling.
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
    setFocusPolicy(Qt::NoFocus);

    // Init layout.
    // We set margins here so that the audio control isn't placed on top of other item buttons.
    QGraphicsGridLayout* layout = new QGraphicsGridLayout();
    layout->setContentsMargins(96, 96, 96, 96);
    layout->setSpacing(0);
    layout->addItem(d->widget, 1, 1);
    layout->setColumnStretchFactor(0, 1);
    layout->setColumnStretchFactor(2, 1);
    layout->setRowStretchFactor(0, 1);
    layout->setRowStretchFactor(2, 1);
    setLayout(layout);
}

AudioSpectrumOverlayWidget::~AudioSpectrumOverlayWidget()
{
}

AudioSpectrumWidget* AudioSpectrumOverlayWidget::audioSpectrumWidget() const {
    return d->widget;
}

} // namespace nx::vms::client::desktop
