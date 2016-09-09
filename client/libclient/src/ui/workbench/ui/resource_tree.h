#pragma once

class QnResourceBrowserWidget;
class QnResizerWidget;
class QGraphicsWidget;
class QnMaskedProxyWidget;
class QnControlBackgroundWidget;
class QnImageButtonWidget;
class HoverFocusProcessor;
class AnimatorGroup;
class VariantAnimator;
class AnimationTimer;
struct QnPaneSettings;
class QnWorkbenchContext;

struct QnWorkbenchUiResourceTree
{
    QnWorkbenchUiResourceTree();

    /** Navigation tree widget. */
    QnResourceBrowserWidget* widget;
    QnResizerWidget* resizerWidget;
    bool ignoreResizerGeometryChanges;
    bool updateResizerGeometryLater;

    /** Proxy widget for navigation tree widget. */
    QnMaskedProxyWidget* item;

    /** Item that provides background for the tree. */
    QnControlBackgroundWidget* backgroundItem;

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

    void initialize(
        QGraphicsWidget* sceneWidget,
        AnimationTimer* animationTimer,
        const QnPaneSettings& settings,
        QnWorkbenchContext* context);

    void setOpened(bool opened, bool animate);
};
