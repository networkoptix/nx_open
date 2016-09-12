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

} //namespace NxUi
