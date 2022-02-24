// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "clock_label.h"

#include <chrono>

#include <QtCore/QDateTime>

#include <ui/common/palette.h>
#include <nx/vms/time/formatter.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace {

using namespace std::chrono;
constexpr auto kClockUpdatePeriod = 1s;

} // namespace

namespace nx::vms::client::desktop::workbench::timeline {

ClockLabel::ClockLabel(QWidget* parent):
    QLabel(parent)
{
    QFont font;
    font.setPixelSize(18);
    font.setWeight(QFont::DemiBold);
    setFont(font);

    setPaletteColor(this, foregroundRole(), colorTheme()->color("timeline.clockText"));

    m_timerId = startTimer(kClockUpdatePeriod);
}

ClockLabel::~ClockLabel()
{
    killTimer(m_timerId);
}

void ClockLabel::timerEvent(QTimerEvent*)
{
    using namespace nx::vms;
    setText(time::toString(QDateTime::currentMSecsSinceEpoch(), time::Format::hh_mm_ss));
}

} // namespace nx::vms::client::desktop::workbench::timeline
