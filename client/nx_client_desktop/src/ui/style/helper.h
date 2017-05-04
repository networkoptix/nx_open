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
        static const int kListRowHeight;                /** QListView minimal row height */
        static const int kViewRowHeight;                /** Other item views minimal row height */
        static const int kSortIndicatorSize;
        static const int kRounding;
        static const QSize kButtonSwitchSize;
        static const QSize kStandaloneSwitchSize;
        static const int kSwitchMargin;                 /**< Margin between switch subcontrol and right side of a control, eg. push button */
        static const QSize kSeparatorSize;              /**< Default separator item size. */
        static const qreal kCheckboxCornerRadius;       /**< Checkbox corner rounding radius */
        static const int kDefaultTopLevelMargin;        /**< Default layout margin for top-level widgets */
        static const QMargins kDefaultTopLevelMargins;  /**< For convenience, kDefaultTopLevelMargin X 4 */
        static const int kDefaultChildMargin;           /**< Default layout margin for child widgets */
        static const QMargins kDefaultChildMargins;     /**< For convenience, kDefaultChildMargin X 4 */
        static const QSize kDefaultLayoutSpacing;       /**< Default layout horizontal and vertical spacing */
        static const int kPanelHeaderHeight;            /**< Height of a panel header, underline excluded */
        static const QMargins kPanelContentMargins;     /**< Panel content margins, underline included, header excluded */
        static const int kGroupBoxTopMargin;            /**< Group box top margin, frame excluded */
        static const QMargins kGroupBoxContentMargins;  /**< Group box content margins, frame width included */
        static const qreal kGroupBoxCornerRadius;       /**< Group box corner rounding radius */
        static const int kDefaultIconSize;              /**< Default size of UI icon */
        static const int kToolTipHeight;                /**< Height of a single line tooltip */
        static const int kMenuButtonIndicatorMargin;    /**< Margin between menu button text and dropdown indicator */
        static const int kTextButtonIconMargin;         /**< Margin between text button icon and text */
        static const int kPushButtonIconMargin;         /**< Margin around push button icon */
        static const int kPushButtonIconOnlyMargin;     /**< Margin around push button icon if button has no text. */
        static const QSize kMinimumDialogSize;          /**< Minimum dialog window size */

        static const int kTabBarFontPixelSize;
        static const int kTextEditFontPixelSize;
        static const int kHeaderViewFontPixelSize;
        static const int kCalendarItemFontPixelSize;
        static const int kCalendarHeaderFontPixelSize;
    };

    class Hints
    {
    public:
        static const qreal kDisabledItemOpacity;        /**< Default disabled item opacity */
        static const qreal kDisabledBrandedButtonOpacity; /**< Branded buttons disabled opacity */
        static const int kMinimumFormLabelWidth;        /**< Minimal text label width in forms */
        static const qreal kMinimumTableRows;           /**< Space for how many rows should minimal table height ensure */
    };

    class Properties
    {
    public:
        static const char* kHoveredRowProperty;     /**< Name of a property to hold index of hovered itemview row (int). */
        static const char* kHoveredIndexProperty;   /**< Name of a property to hold hovered itemview item index (QPersistentModelIndex). */
        static const char* kAccentStyleProperty;    /**< Name of a property to make button brand-colored. */
        static const char* kWarningStyleProperty;   /**< Name of a property to make button warning-colored. */
        static const char* kSliderLength;           /**< Name of a property to change default width of the slider handle. */
        static const char* kSliderFeatures;         /**< Name of a property to add extra slider features. */
        static const char* kDontPolishFontProperty;
        static const char* kTabShape;
        static const char* kSuppressHoverPropery;   /**< Name of a property to suppress hovering of itemview items (bool). */
        static const char* kSideIndentation;        /**< Name of a property to hold overridden leftmost and rightmost itemview item margins (QnIndentation). */
        static const char* kCheckBoxAsButton;       /**< Name of a property to change checkbox appearance to switch button (bool). */
        static const char* kMenuAsDropdown;         /**< Name of a property to change menu appearance to dropdown (bool). */
        static const char* kTabBarIndent;           /**< Name of a property to hold an extra indent of a tab bar. */
        static const char* kItemViewRadioButtons;   /**< Name of a property to change item view checkboxes to radio buttons (bool). */
        static const char* kPushButtonMargin;       /**< Name of a property to hold custom push button margin (int). Buttons with custom margin are left-aligned. */
        static const char* kMenuNoMouseReplayRect;  /**< Name of a property to hold rectangle in global logical coordinates (QRect).
                                                            If menu is closed by click in this rectangle it won't replay mouse event. */
        static const char* kComboBoxPopupWidth;     /**< Name of a property to hold width of combo box popup. */
    };

    /** Flags of additional slider features */
    enum class SliderFeature
    {
        FillingUp   = 0x01  /**< Slider groove from the beginning to the current position is highlighted */
    };

    Q_DECLARE_FLAGS(SliderFeatures, SliderFeature)

    bool isDark(const QColor &color);

    QColor linkColor(const QPalette& palette, bool hovered);

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
