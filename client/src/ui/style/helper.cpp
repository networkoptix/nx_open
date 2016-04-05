#include "helper.h"

#include <private/qfont_p.h>

namespace style
{
    const int Metrics::kStandardPadding = dp(8);
    const int Metrics::kMenuItemHPadding = dp(12);
    const int Metrics::kMenuItemVPadding = dp(5);
    const int Metrics::kMenuItemTextLeftPadding = dp(28);
    const int Metrics::kArrowSize = dp(8);
    const int Metrics::kCheckIndicatorSize = dp(16);
    const int Metrics::kExclusiveIndicatorSize = dp(16);
    const int Metrics::kMenuCheckSize = dp(16);
    const int Metrics::kMinimumButtonWidth = dp(80);
    const int Metrics::kButtonHeight = dp(28);
    const int Metrics::kHeaderSize = dp(32);
    const int Metrics::kViewRowHeight = dp(24);
    const int Metrics::kSortIndicatorSize = dp(14);
    const int Metrics::kRounding = dp(1);
    const QSize Metrics::kSwitchSize(dp(30), dp(15));

    const char *Properties::kHoveredRowProperty = "_qn_hoveredRow";
    const char *Properties::kAccentStyleProperty = "_qn_accentStyle";
    const char *Properties::kSliderLength = "_qn_sliderLength";
    const char *Properties::kDontPolishFontProperty = "_qn_dontPolishFont";

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
        return dpr(value);
    }

    bool isDark(const QColor &color)
    {
        return color.toHsl().lightness() < 128;
    }

    bool isTabRounded(QTabBar::Shape shape)
    {
        return shape <= QTabBar::RoundedEast;
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
