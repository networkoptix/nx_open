// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "helper.h"

#include <QtCore/QVariant>
#include <QtGui/private/qfont_p.h>
#include <QtWidgets/QWidget>

namespace nx::style {

const char* Properties::kHoveredRowProperty = "_qn_hoveredRow";
const char* Properties::kHoveredIndexProperty = "_qn_hoveredIndex";
const char* Properties::kAccentStyleProperty = "_qn_accentStyle";
const char* Properties::kWarningStyleProperty = "_qn_warningStyle";
const char* Properties::kSliderLength = "_qn_sliderLength";
const char* Properties::kSliderFeatures = "_qn_sliderFeatures";
const char* Properties::kDontPolishFontProperty = "_qn_dontPolishFont";
const char* Properties::kTabShape = "_qn_tabShape";
const char* Properties::kSuppressHoverPropery = "_qn_suppressHover";
const char* Properties::kSideIndentation = "_qn_sideIndentation";
const char* Properties::kCheckBoxAsButton = "_qn_checkBoxAsButton";
const char* Properties::kMenuAsDropdown = "_qn_menuAsDropdown";
const char* Properties::kTabBarIndent = "_qn_tabBarIndent";
const char* Properties::kTabBarShift = "_qn_tabBarShift";
const char* Properties::kItemViewRadioButtons = "_qn_itemViewRadioButtons";
const char* Properties::kPushButtonMargin = "_qn_pushButtonMargin";
const char* Properties::kTextButtonBackgroundProperty = "_qn_textButtonBackground";
const char* Properties::kMenuNoMouseReplayArea = "_qn_menuNoMouseReplayArea";
const char* Properties::kComboBoxPopupWidth = "_qn_comboBoxPopupWidth";
const char* Properties::kGroupBoxContentTopMargin = "_qn_groupBoxContentTopMargin";
const char* Properties::kIsCloseTabButton = "_qn_isCloseTabButton";
const char* Properties::kCustomTextButtonIconSpacing = "_qn_customTextButtonIconSpacing";

bool isDark(const QColor& color)
{
    return color.toHsl().lightness() < 128;
}

QColor linkColor(const QPalette& palette, bool hovered)
{
    return palette.color(hovered ? QPalette::LinkVisited : QPalette::Link);
}

bool isAccented(const QWidget* widget)
{
    return widget && widget->property(Properties::kAccentStyleProperty).toBool();
}

bool isWarningStyle(const QWidget* widget)
{
    return widget && widget->property(Properties::kWarningStyleProperty).toBool();
}

bool hasTextButtonBackgroundStyle(const QWidget* widget)
{
    return widget && widget->property(Properties::kTextButtonBackgroundProperty).toBool();
}

RectCoordinates::RectCoordinates(const QRectF& rect):
    rect(rect)
{
}

qreal RectCoordinates::x(qreal x)
{
    return rect.isEmpty() ? 0 : rect.left() + rect.width() * x;
}

qreal RectCoordinates::y(qreal y)
{
    return rect.isEmpty() ? 0 : rect.top() + rect.height() * y;
}

TabShape tabShape(const QWidget* widget)
{
    if (!widget)
        return TabShape::Default;

    return static_cast<TabShape>(widget->property(Properties::kTabShape).toInt());
}

int textButtonIconSpacing(const QWidget* widget)
{
    bool ok = false;
    const int spacing = widget->property(Properties::kCustomTextButtonIconSpacing).toInt(&ok);
    return ok ? spacing : Metrics::kTextButtonIconSpacing;
}

} // namespace nx::style
