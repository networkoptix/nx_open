// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_counter_label.h"

#include <QtCore/QVariant>

#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/palette.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kFontPixelSize = 8;
static constexpr int kFontWeight = QFont::Bold;
static constexpr int kFixedHeight = 12;
static const QMargins kContentsMargins(4, 0, 3, 0); //< Manually adjusted for the best appearance.

} // namespace

NotificationCounterLabel::NotificationCounterLabel(QWidget* parent): base_type(parent)
{
    setFixedHeight(kFixedHeight);
    setMinimumWidth(kFixedHeight);
    setRoundingRadius(kFixedHeight / 2.0, false);
    setContentsMargins(kContentsMargins);
    setProperty(style::Properties::kDontPolishFontProperty, true);

    setStyleSheet(QStringLiteral("QLabel { color: %1; }")
        .arg(colorTheme()->color("dark3").name()));

    QFont font(this->font());
    font.setPixelSize(kFontPixelSize);
    font.setWeight(kFontWeight);
    setFont(font);
}

} // namespace nx::vms::client::desktop
