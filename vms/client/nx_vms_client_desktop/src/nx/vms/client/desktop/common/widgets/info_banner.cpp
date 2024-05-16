// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "info_banner.h"

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <utils/math/color_transformations.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::client::core;

InfoBanner::InfoBanner(QWidget* parent, Qt::WindowFlags f):
    base_type(parent, f)
{
    setupUi();
}

InfoBanner::InfoBanner(const QString& text, QWidget* parent, Qt::WindowFlags f):
    base_type(text, parent, f)
{
    setupUi();
}

void InfoBanner::setWarningStyle(bool value)
{
    if (value == m_warningStyle)
        return;

    m_warningStyle = value;
    setWarningStyleInternal(value);
}

void InfoBanner::setupUi()
{
    setContentsMargins(8, 2, 8, 2);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    setProperty(style::Properties::kDontPolishFontProperty, true);
    auto font = this->font();
    font.setPixelSize(12);
    setFont(font);

    setWarningStyleInternal(m_warningStyle);
}

void InfoBanner::setWarningStyleInternal(bool value)
{
    static const QString kStyleSheetTemplate = R"css(
        border-radius: 2px;
        background-color: %1;
        color: %2;
    )css";

    const QColor bgColor = value
        ? toTransparent(core::colorTheme()->color("red_l2"), 0.2)
        : core::colorTheme()->color("dark10");

    const QColor textColor = value
        ? core::colorTheme()->color("red_l2")
        : core::colorTheme()->color("light10");

    const QString styleSheet = kStyleSheetTemplate.arg(
        bgColor.name(QColor::HexArgb),
        textColor.name(QColor::HexArgb));

    setStyleSheet(styleSheet);
}

} // namespace nx::vms::client::desktop
