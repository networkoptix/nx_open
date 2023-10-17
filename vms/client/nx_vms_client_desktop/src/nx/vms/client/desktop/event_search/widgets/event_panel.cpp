// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_panel.h"

#include <QtGui/QMouseEvent>

#include <nx/vms/client/core/skin/color_theme.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/workbench/workbench_display.h>

#include "private/event_panel_p.h"

namespace nx::vms::client::desktop {

EventPanel::EventPanel(WindowContext* context, QWidget* parent):
    base_type(parent),
    WindowContextAware(context),
    d(new Private(this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) || !defined(Q_OS_MACOS)
        setAutoFillBackground(true);
    #endif

    setPaletteColor(this, QPalette::Dark, core::colorTheme()->color("dark4"));
    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dark5"));
    setPaletteColor(this, QPalette::Light, core::colorTheme()->color("dark8"));
    setPaletteColor(this, QPalette::Midlight, core::colorTheme()->color("dark9", 51));
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
