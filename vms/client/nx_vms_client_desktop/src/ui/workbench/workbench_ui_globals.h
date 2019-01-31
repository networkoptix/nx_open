#pragma once

namespace nx::vms::client::desktop {

/** Default panel shadow */
static const qreal kShadowThickness = 2.0;

/** Hover mouse over 'Show' button for this time to see unpinned panel. */
static const int kOpenPanelTimeoutMs = 150;

/** Wait this time to make unpinned calendar panel auto-hide. */
static const int kCloseCalendarPanelTimeoutMs = 2000;

/** Wait this time to make unpinned side (resource or notification) panel auto-hide. */
static const int kCloseSidePanelTimeoutMs = 1500;

/** Time span to avoid graphics button double click where we don't need it. */
static const int kButtonInactivityTimeoutMs = 300;

/** Opacity value for visible items. */
static const qreal kOpaque = 1.0;

/** Opacity value for hidden items. */
static const qreal kHidden = 0.0;

/** Hide panels by 1 pixel behind the screen borders - just in case. */
static const qreal kHidePanelOffset = 1.0;

/**
 * Z-order for workbench panel controls.
 */
enum ItemZOrder
{
    BackgroundItemZOrder,
    ContentItemZOrder,
    ResizerItemZOrder,
    ControlItemZOrder,
    ShadowItemZOrder,
    TooltipItemZOrder,
    CursorTooltipItemZOrder,
};

} // namespace nx::vms::client::desktop
