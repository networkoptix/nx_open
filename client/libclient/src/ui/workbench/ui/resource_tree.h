#pragma once

class QnResourceBrowserWidget;
class QGraphicsWidget;
class QnMaskedProxyWidget;
class QnFramedWidget;
class QnImageButtonWidget;
class HoverFocusProcessor;
class AnimatorGroup;
class VariantAnimator;

struct QnWorkbenchUiResourceTree
{
    QnWorkbenchUiResourceTree();

    /** Navigation tree widget. */
    QnResourceBrowserWidget* widget;
    QGraphicsWidget* resizerWidget;
    bool ignoreResizerGeometryChanges;
    bool updateResizerGeometryLater;

    /** Proxy widget for navigation tree widget. */
    QnMaskedProxyWidget* item;

    /** Item that provides background for the tree. */
    QnFramedWidget* backgroundItem;

    /** Button to show/hide the tree. */
    QnImageButtonWidget* showButton;

    /** Button to pin the tree. */
    QnImageButtonWidget* pinButton;

    /** Hover processor that is used to hide the tree when the mouse leaves it. */
    HoverFocusProcessor* hidingProcessor;

    /** Hover processor that is used to show the tree when the mouse hovers over it. */
    HoverFocusProcessor* showingProcessor;

    /** Hover processor that is used to change tree opacity when mouse hovers over it. */
    HoverFocusProcessor* opacityProcessor;

    /** Animator group for tree's opacity. */
    AnimatorGroup* opacityAnimatorGroup;

    /** Animator for tree's position. */
    VariantAnimator* xAnimator;
};
