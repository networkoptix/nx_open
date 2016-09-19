#include "helper.h"

#include <private/qfont_p.h>

namespace style
{
    const int Metrics::kStandardPadding = dp(8);
    const int Metrics::kMenuItemHPadding = dp(12);
    const int Metrics::kMenuItemVPadding = dp(5);
    const int Metrics::kMenuItemTextLeftPadding = dp(28);
    const int Metrics::kArrowSize = dp(8);
    const int Metrics::kCrossSize = dp(8);
    const int Metrics::kSpinButtonWidth = dp(16);
    const int Metrics::kCheckIndicatorSize = dp(16);
    const int Metrics::kExclusiveIndicatorSize = dp(16);
    const int Metrics::kMenuCheckSize = dp(16);
    const int Metrics::kMinimumButtonWidth = dp(80);
    const int Metrics::kButtonHeight = dp(28);
    const int Metrics::kHeaderSize = dp(32);
    const int Metrics::kViewRowHeight = dp(24);
    const int Metrics::kSortIndicatorSize = dp(14);
    const int Metrics::kRounding = dp(1);
    const QSize Metrics::kButtonSwitchSize(dp(33), dp(17));
    const QSize Metrics::kStandaloneSwitchSize(dp(30), dp(15));
    const int Metrics::kSwitchMargin = dp(4);
    const QSize Metrics::kSeparatorSize(dp(10), dp(8));
    const qreal Metrics::kCheckboxCornerRadius = dp(0.2); // for lightly antialiased corners

    const int Metrics::kDefaultTopLevelMargin = dp(16);
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

    const QSize Metrics::kDefaultLayoutSpacing(dp(8), dp(8));
    const int Metrics::kPanelHeaderHeight = dp(24);
    const QMargins Metrics::kPanelContentMargins(0, dp(12), 0, 0);
    const int Metrics::kGroupBoxTopMargin = dp(8);
    const QMargins Metrics::kGroupBoxContentMargins(Metrics::kDefaultTopLevelMargins);
    const qreal Metrics::kGroupBoxCornerRadius = dp(2.0);
    const int Metrics::kDefaultIconSize = 20;

    const int Metrics::kToolTipHeight = 24;

    const int Metrics::kMenuButtonIndicatorMargin = 2;
    const int Metrics::kTextButtonIconMargin = 2;

    const qreal Hints::kDisabledItemOpacity = 0.3;

    const char* Properties::kHoveredRowProperty = "_qn_hoveredRow";
    const char* Properties::kHoveredIndexProperty = "_qn_hoveredIndex";
    const char* Properties::kAccentStyleProperty = "_qn_accentStyle";
    const char* Properties::kSliderLength = "_qn_sliderLength";
    const char* Properties::kSliderFeatures = "_qn_sliderFeatures";
    const char* Properties::kDontPolishFontProperty = "_qn_dontPolishFont";
    const char* Properties::kTabShape = "_qn_tabShape";
    const char* Properties::kSuppressHoverPropery = "_qn_suppressHover";
    const char* Properties::kSideIndentation = "_qn_sideIndentation";
    const char* Properties::kCheckBoxAsButton = "_qn_checkBoxAsButton";
    const char* Properties::kTabBarIndent = "_qn_tabBarIndent";

    //TODO: #vkutin Refactor this property to something else in the future
    const char* Properties::kButtonMarginProperty = "_qn_buttonMargin";

    qreal dpr(qreal value)
    {
    #ifdef Q_OS_MAC
        // On mac the DPI is always 72 so we should not scale it
        return value;
    #else
        static const qreal scale = qreal(qt_defaultDpiX()) / 96.0;
        return value * scale;
    #endif
    }

    int dp(qreal value)
    {
        //TODO #vkutin #common Completely remove dp() when we are ABSOLUTELY sure it's not needed
        return value; // dpr(value);
    }

    bool isDark(const QColor &color)
    {
        return color.toHsl().lightness() < 128;
    }


    RectCoordinates::RectCoordinates(const QRectF &rect)
        : rect(rect)
    {}

    qreal RectCoordinates::x(qreal x)
    {
        if (rect.isEmpty())
            return 0;
        return rect.left() + rect.width() * x;
    }

    qreal RectCoordinates::y(qreal y)
    {
        if (rect.isEmpty())
            return 0;
        return rect.top() + rect.height() * y;
    }
}
