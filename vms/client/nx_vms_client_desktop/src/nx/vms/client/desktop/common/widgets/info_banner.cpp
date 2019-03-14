#include "info_banner.h"

#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/style/helper.h>
#include <utils/math/color_transformations.h>

namespace nx::vms::client::desktop {

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
        ? toTransparent(colorTheme()->color("red_l2"), 0.2)
        : colorTheme()->color("dark10");

    const QColor textColor = value
        ? colorTheme()->color("red_l2")
        : colorTheme()->color("light10");

    const QString styleSheet = kStyleSheetTemplate.arg(
        bgColor.name(QColor::HexArgb),
        textColor.name(QColor::HexArgb));

    setStyleSheet(styleSheet);
}

} // namespace nx::vms::client::desktop
