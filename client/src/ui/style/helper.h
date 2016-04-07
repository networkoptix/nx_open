#pragma once

#include <QtWidgets/QTabBar>

namespace style
{
    class Metrics
    {
    public:
        static const int kStandardPadding;
        static const int kMenuItemHPadding;
        static const int kMenuItemVPadding;
        static const int kMenuItemTextLeftPadding;
        static const int kArrowSize;
        static const int kCrossSize;
        static const int kCheckIndicatorSize;
        static const int kExclusiveIndicatorSize;
        static const int kMenuCheckSize;
        static const int kMinimumButtonWidth;
        static const int kButtonHeight;
        static const int kHeaderSize;
        static const int kViewRowHeight;
        static const int kSortIndicatorSize;
        static const int kRounding;
        static const QSize kSwitchSize;
    };

    class Properties
    {
    public:
        static const char *kHoveredRowProperty;
        static const char *kAccentStyleProperty;
        static const char *kSliderLength;           /**< Name of a property to change default width of the slider handle. */
        static const char *kSliderFeatures;         /**< Name of a property to add extra slider features. */
        static const char *kDontPolishFontProperty;
        static const char *kTabShape;
    };

    /** Flags of additional slider features */
    enum class SliderFeature
    {
        FillingUp   = 0x01  /**< Slider groove from the beginning to the current position is highlighted */
    };

    Q_DECLARE_FLAGS(SliderFeatures, SliderFeature)

    qreal dpr(qreal value);
    int dp(qreal value);

    bool isDark(const QColor &color);

    class RectCoordinates
    {
        QRectF rect;

    public:
        RectCoordinates(const QRectF &rect);
        qreal x(qreal x);
        qreal y(qreal y);
    };

    enum Direction
    {
        Up,
        Down,
        Left,
        Right
    };

    enum class TabShape
    {
        Default,
        Compact,
        Rectangular
    };
}

Q_DECLARE_METATYPE(style::TabShape)
