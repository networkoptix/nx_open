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
        static const int kSpinButtonWidth;
        static const int kCheckIndicatorSize;
        static const int kExclusiveIndicatorSize;
        static const int kMenuCheckSize;
        static const int kMinimumButtonWidth;
        static const int kButtonHeight;
        static const int kHeaderSize;
        static const int kViewRowHeight;
        static const int kSortIndicatorSize;
        static const int kRounding;
        static const QSize kButtonSwitchSize;
        static const QSize kStandaloneSwitchSize;
        static const int kSwitchMargin;                 /**< Margin between switch subcontrol and right side of a control, eg. push button */
        static const QSize kSeparatorSize;              /**< Default separator item size. */
        static const qreal kCheckboxCornerRadius;       /**< Checkbox corner rounding radius */
        static const int kDefaultTopLevelMargin;        /**< Default layout margin for top-level widgets */
        static const int kDefaultChildMargin;           /**< Default layout margin for child widgets */
        static const QSize kDefaultLayoutSpacing;       /**< Default layout horizontal and vertical spacing */
        static const int kPanelHeaderHeight;            /**< Height of a panel header, underline excluded */
        static const QMargins kPanelContentMargins;     /**< Panel content margins, underline included, header excluded */
        static const int kGroupBoxTopMargin;            /**< Group box top margin, frame excluded */
        static const QMargins kGroupBoxContentMargins;  /**< Group box content margins, frame width included */
        static const qreal kGroupBoxCornerRadius;       /**< Group box corner rounding radius */
    };

    class Properties
    {
    public:
        static const char *kHoveredRowProperty;
        static const char *kHoveredIndexProperty;
        static const char *kAccentStyleProperty;
        static const char *kSliderLength;           /**< Name of a property to change default width of the slider handle. */
        static const char *kSliderFeatures;         /**< Name of a property to add extra slider features. */
        static const char *kDontPolishFontProperty;
        static const char *kTabShape;
        static const char *kSuppressHoverPropery;   /**< Name of a property to suppress hovering of itemview items. */
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
