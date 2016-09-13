#pragma once

namespace NxUi {

/** Default panel shadow */
static const qreal kShadowThickness = 2.0;

/** Hover mouse over 'Show' button for this time to see unpinned panel. */
static const int kOpenPanelTimeoutMs = 250;

/** Wait this time to make unpinned panel auto-hide. */
static const int kClosePanelTimeoutMs = 2000;

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
};

} // namespace NxUi
