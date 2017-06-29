#include "helper.h"

#include <QtGui/private/qfont_p.h>

namespace style
{
    const int Metrics::kStandardPadding = 8;
    const int Metrics::kMenuItemHPadding = 12;
    const int Metrics::kMenuItemVPadding = 5;
    const int Metrics::kMenuItemTextLeftPadding = 28;
    const int Metrics::kArrowSize = 8;
    const int Metrics::kCrossSize = 8;
    const int Metrics::kSpinButtonWidth = 16;
    const int Metrics::kCheckIndicatorSize = 16;
    const int Metrics::kExclusiveIndicatorSize = 16;
    const int Metrics::kMenuCheckSize = 16;
    const int Metrics::kMinimumButtonWidth = 80;
    const int Metrics::kButtonHeight = 28;
    const int Metrics::kHeaderSize = 32;
    const int Metrics::kListRowHeight = 20;
    const int Metrics::kViewRowHeight = 24;
    const int Metrics::kSortIndicatorSize = 14;
    const int Metrics::kRounding = 1;
    const QSize Metrics::kButtonSwitchSize(33, 17);
    const QSize Metrics::kStandaloneSwitchSize(30, 15);
    const int Metrics::kSwitchMargin = 4;
    const QSize Metrics::kSeparatorSize(10, 8);
    const qreal Metrics::kCheckboxCornerRadius = 0.2; // for lightly antialiased corners

    const int Metrics::kDefaultTopLevelMargin = 16;
    const QMargins Metrics::kDefaultTopLevelMargins(
        Metrics::kDefaultTopLevelMargin,
        Metrics::kDefaultTopLevelMargin,
        Metrics::kDefaultTopLevelMargin,
        Metrics::kDefaultTopLevelMargin);

    const int Metrics::kDefaultChildMargin = 0;
    const QMargins Metrics::kDefaultChildMargins(
        Metrics::kDefaultChildMargin,
        Metrics::kDefaultChildMargin,
        Metrics::kDefaultChildMargin,
        Metrics::kDefaultChildMargin);

    const QSize Metrics::kDefaultLayoutSpacing(8, 8);
    const int Metrics::kPanelHeaderHeight = 24;
    const QMargins Metrics::kPanelContentMargins(0, 12, 0, 0);
    const int Metrics::kGroupBoxTopMargin = 8;
    const QMargins Metrics::kGroupBoxContentMargins(Metrics::kDefaultTopLevelMargins);
    const qreal Metrics::kGroupBoxCornerRadius = 2.0;
    const int Metrics::kDefaultIconSize = 20;

    const int Metrics::kToolTipHeight = 24;

    const int Metrics::kMenuButtonIndicatorMargin = 2;
    const int Metrics::kTextButtonIconMargin = 2;

    const int Metrics::kPushButtonIconMargin = 6;
    const int Metrics::kPushButtonIconOnlyMargin = 5;

    const QSize Metrics::kMinimumDialogSize(400, 64);

    const int Metrics::kTabBarFontPixelSize = 12;
    const int Metrics::kTextEditFontPixelSize = 14;
    const int Metrics::kHeaderViewFontPixelSize = 14;
    const int Metrics::kCalendarItemFontPixelSize = 12;
    const int Metrics::kCalendarHeaderFontPixelSize = 11;

    const qreal Hints::kDisabledItemOpacity = 0.3;
    const qreal Hints::kDisabledBrandedButtonOpacity = 0.1;
    const int Hints::kMinimumFormLabelWidth = 64 - Metrics::kDefaultTopLevelMargin;
    const qreal Hints::kMinimumTableRows = 3.0;

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
    const char* Properties::kItemViewRadioButtons = "_qn_itemViewRadioButtons";
    const char* Properties::kPushButtonMargin = "_qn_pushButtonMargin";
    const char* Properties::kMenuNoMouseReplayRect = "_qn_menuNoMouseReplayRect";
    const char* Properties::kComboBoxPopupWidth = "_qn_comboBoxPopupWidth";
    const char* Properties::kGroupBoxContentTopMargin = "_qn_groupBoxContentTopMargin";

    bool isDark(const QColor& color)
    {
        return color.toHsl().lightness() < 128;
    }

    QColor linkColor(const QPalette& palette, bool hovered)
    {
        return palette.color(hovered ? QPalette::LinkVisited : QPalette::Link);
    }

    RectCoordinates::RectCoordinates(const QRectF& rect):
        rect(rect)
    {
    }

    qreal RectCoordinates::x(qreal x)
    {
        return rect.isEmpty()
            ? 0
            : rect.left() + rect.width() * x;
    }

    qreal RectCoordinates::y(qreal y)
    {
        return rect.isEmpty()
            ? 0
            : rect.top() + rect.height() * y;
    }
}
