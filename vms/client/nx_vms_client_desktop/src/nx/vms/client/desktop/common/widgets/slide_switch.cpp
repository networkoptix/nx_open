// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "slide_switch.h"

#include <QtWidgets/QStyleOptionButton>
#include <QtWidgets/QStylePainter>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>

namespace nx::vms::client::desktop {

SlideSwitch::SlideSwitch(QWidget* parent):
    base_type(parent)
{
}

QSize SlideSwitch::minimumSizeHint() const
{
    return nx::style::Metrics::kStandaloneSwitchSize;
}

QSize nx::vms::client::desktop::SlideSwitch::sizeHint() const
{
    return minimumSizeHint();
}

void SlideSwitch::paintEvent(QPaintEvent*)
{
    QStylePainter painter(this);
    QStyleOptionButton styleOption;
    initStyleOption(&styleOption);
    Style::instance()->drawSwitch(&painter, &styleOption, this);
}

} // namespace nx::vms::client::desktop
