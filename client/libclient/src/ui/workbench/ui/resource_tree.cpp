#include "resource_tree.h"

QnWorkbenchUiResourceTree::QnWorkbenchUiResourceTree():
    widget(nullptr),
    resizerWidget(nullptr),
    ignoreResizerGeometryChanges(false),
    updateResizerGeometryLater(false),
    item(nullptr),
    backgroundItem(nullptr),
    showButton(nullptr),
    pinButton(nullptr),
    hidingProcessor(nullptr),
    showingProcessor(nullptr),
    opacityProcessor(nullptr),
    opacityAnimatorGroup(nullptr),
    xAnimator(nullptr)
{
}
