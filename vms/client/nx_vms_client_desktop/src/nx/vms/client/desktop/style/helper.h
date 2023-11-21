// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFlags>
#include <QtCore/QMargins>
#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtGui/QColor>
#include <QtGui/QPalette>

class QWidget;

namespace nx::style {

struct Metrics
{
    static constexpr int kStandardPadding = 8;
    static constexpr int kInterSpace = 4;
    static constexpr int kMenuItemHPadding = 12;
    static constexpr int kMenuItemVPadding = 5;
    static constexpr int kMenuItemTextLeftPadding = 28;
    static constexpr int kSubmenuArrowPadding = 4;
    static constexpr int kArrowSize = 8;
    static constexpr int kCrossSize = 8;
    static constexpr int kSpinButtonWidth = 16;
    static constexpr int kCheckIndicatorSize = 16;
    static constexpr int kExclusiveIndicatorSize = 16;
    static constexpr int kMenuCheckSize = 16;
    static constexpr int kMinimumButtonWidth = 80;
    static constexpr int kButtonHeight = 28;
    static constexpr int kHeaderSize = 32;

    /** QListView minimal row height. */
    static constexpr int kListRowHeight = 20;

    /** QComboBoxListView minimal row height. */
    static constexpr int kComboBoxDropdownRowHeight = 24;

    /** Other item views minimal row height. */
    static constexpr int kViewRowHeight = 24;

    /** Recommended separator row height for non-uniform row height item views. */
    static constexpr int kViewSeparatorRowHeight = 16;

    /**
     * Row height of non-interactable empty item which placed between header and pinned items
     * in widget-based resource item views for proper alignment.
     */
    static constexpr int kViewSpacerRowHeight = 8;

    static constexpr int kSortIndicatorSize = 14;
    static constexpr int kRounding = 1;

    static constexpr QSize kButtonSwitchSize{33, 17};
    static constexpr QSize kStandaloneSwitchSize{30, 15};

    /** Margin between switch subcontrol and right side of a control, eg. push button. */
    static constexpr int kSwitchMargin = 4;

    /** Default separator item size. */
    static constexpr QSize kSeparatorSize{10, 8};

    /** Checkbox corner rounding radius (for lightly antialiased corners). */
    static constexpr qreal kCheckboxCornerRadius = 0.2;

    /** Default layout margin for top-level widgets. */
    static constexpr int kDefaultTopLevelMargin = 16;

    /** Convenience helper. */
    static constexpr QMargins kDefaultTopLevelMargins{
        kDefaultTopLevelMargin,
        kDefaultTopLevelMargin,
        kDefaultTopLevelMargin,
        kDefaultTopLevelMargin
    };

    /** Default layout margin for child widgets. */
    static constexpr int kDefaultChildMargin = 0;

    /** Convenience helper. */
    static constexpr QMargins kDefaultChildMargins{
        kDefaultChildMargin,
        kDefaultChildMargin,
        kDefaultChildMargin,
        kDefaultChildMargin
    };

    /** Default layout horizontal and vertical spacing. */
    static constexpr QSize kDefaultLayoutSpacing{8, 8};

    /** Height of a panel header, underline excluded. */
    static constexpr int kPanelHeaderHeight = 24;

    /** Panel content margins, underline included, header excluded. */
    static constexpr QMargins kPanelContentMargins{0, 12, 0, 0};

    /** Group box top margin, frame excluded. */
    static constexpr int kGroupBoxTopMargin = 8;

    /** Group box content margins, frame width included. */
    static constexpr QMargins kGroupBoxContentMargins{kDefaultTopLevelMargins};

    static constexpr QMargins kMessageBarContentMargins{16, 10, 16, 10};

    /** Group box corner rounding radius. */
    static constexpr qreal kGroupBoxCornerRadius = 2.0;

    /** Default size of UI icon. */
    static constexpr int kDefaultIconSize = 20;

    /** Height of a single line tooltip. */
    static constexpr int kToolTipHeight = 24;

    /** Margin between menu button text and dropdown indicator. */
    static constexpr int kMenuButtonIndicatorMargin = 2;

    /** Margin around text button elements if the button has background. */
    static constexpr int kTextButtonBackgroundMargin = 6;

    /** Spacing between text button icon and text. */
    static constexpr int kTextButtonIconSpacing = 2;

    /** Margin around push button icon. */
    static constexpr int kPushButtonIconMargin = 6;

    /** Margin around push button icon if button has no text. */
    static constexpr int kPushButtonIconOnlyMargin = 5;

    /** Minimum dialog window size. */
    static constexpr QSize kMinimumDialogSize{400, 64};

    /** Space between a widget and its hint button. */
    static constexpr int kHintButtonMargin = 6;

    static constexpr int kTabBarFontPixelSize = 12;
    static constexpr int kTextEditFontPixelSize = 14;
    static constexpr int kHeaderViewFontPixelSize = 14;
    static constexpr int kCalendarItemFontPixelSize = 12;
    static constexpr int kCalendarHeaderFontPixelSize = 12;
    static constexpr int kBannerLabelFontPixelSize = 14;

    // TODO: #vbreus Schedule cell size set to fixed size of 32 pixels, likely there is no need
    // to make it resizable.
    /** Minimum size of single square cell in the schedule grid. */
    static constexpr int kScheduleGridMinimumCellSize = 32;

    /** Maximum size of single square cell in the schedule grid. */
    static constexpr int kScheduleGridMaximumCellSize = 32;

    /** Default size for one of rewind arrows */
    static constexpr QSize kRewindArrowSize{24, 32};
};

struct Hints
{
    /** Default disabled item opacity. */
    static constexpr qreal kDisabledItemOpacity = 0.3;

    /** Branded buttons disabled opacity. */
    static constexpr qreal kDisabledBrandedButtonOpacity = 0.1;

    /** Minimal text label width in forms. */
    static constexpr int kMinimumFormLabelWidth = 64 - Metrics::kDefaultTopLevelMargin;

    /** Space for how many rows should minimal table height ensure. */
    static constexpr int kMinimumTableRows = 3;
};

struct Properties
{
    /** Name of a property to hold index of hovered itemview row (int). */
    static const char* kHoveredRowProperty;

    /** Name of a property to hold hovered itemview item index (QPersistentModelIndex). */
    static const char* kHoveredIndexProperty;

    /** Name of a property to make button brand-colored. */
    static const char* kAccentStyleProperty;

    /** Name of a property to make button warning-colored. */
    static const char* kWarningStyleProperty;

    /** Name of a property to change default width of the slider handle. */
    static const char* kSliderLength;

    /** Name of a property to add extra slider features. */
    static const char* kSliderFeatures;

    static const char* kDontPolishFontProperty;

    static const char* kTabShape;

    /** Name of a property to suppress hovering of itemview items (bool). */
    static const char* kSuppressHoverPropery;

    /**
     * Name of a property to hold overridden leftmost and rightmost itemview item margins
     * (QnIndentation).
     */
    static const char* kSideIndentation;

    /** Name of a property to change checkbox appearance to switch button (bool). */
    static const char* kCheckBoxAsButton;

    /** Name of a property to change menu appearance to dropdown (bool). */
    static const char* kMenuAsDropdown;

    /** Name of a property to hold an extra horizontal shift of a tab bar. */
    static const char* kTabBarShift;

    /** Name of a property to hold extra left and right indents of a tab bar. */
    static const char* kTabBarIndent;

    /** Name of a property to change item view checkboxes to radio buttons (bool). */
    static const char* kItemViewRadioButtons;

    /**
     * Name of a property to hold custom push button margin (int). Buttons with custom margin are
     * left-aligned.
     */
    static const char* kPushButtonMargin;

    /** Name of a property to enable drawing of text button background. */
    static const char* kTextButtonBackgroundProperty;

    /**
     * Name of a property to hold either rectangle in global coordinates (QRect) or a widget
     * (QPointerQWidget>). If menu is closed by click in this area it won't replay mouse event.
     */
    static const char* kMenuNoMouseReplayArea;

    /** Name of a property to hold width of combo box popup. */
    static const char* kComboBoxPopupWidth;

    /** Name of a property to hold group box top content margins without header (int). */
    static const char* kGroupBoxContentTopMargin;

    /**
     *  Name of a property to determine if object belongs to CloseTabButton class or its
     *  descendant.
    */
    static const char* kIsCloseTabButton;
};

/** Flags of additional slider features */
enum class SliderFeature
{
    /** Slider groove from the beginning to the current position is highlighted. */
    FillingUp = 0x01
};
Q_DECLARE_FLAGS(SliderFeatures, SliderFeature)

bool isDark(const QColor& color);

QColor linkColor(const QPalette& palette, bool hovered);

bool isAccented(const QWidget* widget);
bool isWarningStyle(const QWidget* widget);
bool hasTextButtonBackgroundStyle(const QWidget* widget);

class RectCoordinates
{
    QRectF rect;

public:
    RectCoordinates(const QRectF& rect);
    qreal x(qreal x);
    qreal y(qreal y);
};

enum class Direction
{
    up,
    down,
    left,
    right
};

enum class TabShape
{
    Default,
    Compact,
    Rectangular
};

TabShape tabShape(const QWidget* widget);

} // namespace nx::style
