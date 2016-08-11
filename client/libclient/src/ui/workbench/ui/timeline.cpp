#include "timeline.h"

namespace {

static const int kDefaultThumbnailsHeight = 48;

}

QnWorkbenchUiTimeline::QnWorkbenchUiTimeline():
    visible(false),
    pinned(false),
    opened(false),
    item(nullptr),
    resizerWidget(nullptr),
    ignoreResizerGeometryChanges(false),
    updateResizerGeometryLater(false),
    zoomingIn(false),
    zoomingOut(false),
    zoomButtonsWidget(nullptr),
    opacityProcessor(nullptr),
    yAnimator(nullptr),
    showButton(nullptr),
    showWidget(nullptr),
    showingProcessor(nullptr),
    hidingProcessor(nullptr),
    opacityAnimatorGroup(nullptr),
    autoHideTimer(nullptr),
    lastThumbnailsHeight(kDefaultThumbnailsHeight)
{
}
