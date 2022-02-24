// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_panel.h"
#include "private/event_panel_p.h"

#include <QtGui/QMouseEvent>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/workbench/workbench_display.h>
#include <ui/common/palette.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace nx::vms::client::desktop {

EventPanel::EventPanel(QnWorkbenchContext* context, QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    d(new Private(this))
{
    setPaletteColor(this, QPalette::Dark, colorTheme()->color("dark4"));
    setPaletteColor(this, QPalette::Window, colorTheme()->color("dark5"));
    setPaletteColor(this, QPalette::Light, colorTheme()->color("dark8"));
    setPaletteColor(this, QPalette::Midlight, colorTheme()->color("dark9", 51));
}

EventPanel::~EventPanel()
{
}

void EventPanel::mousePressEvent(QMouseEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress && event->button() == Qt::RightButton)
        event->accept();
}

} // namespace nx::vms::client::desktop
