#pragma once

class QnNavigationItem;
class QnResizerWidget;
class QGraphicsWidget;
class HoverFocusProcessor;
class VariantAnimator;
class QnImageButtonWidget;
class AnimatorGroup;

struct QnWorkbenchUiTimeline
{
    QnWorkbenchUiTimeline();

    bool visible;
    bool pinned;
    bool opened;

    QnNavigationItem* item;
    QnResizerWidget* resizerWidget;

    bool ignoreResizerGeometryChanges;
    bool updateResizerGeometryLater;

    bool zoomingIn;
    bool zoomingOut;

    QGraphicsWidget* zoomButtonsWidget;

    /** Hover processor that is used to change  opacity when mouse is hovered over it. */
    HoverFocusProcessor* opacityProcessor;

    /** Animator for  position. */
    VariantAnimator* yAnimator;

    QnImageButtonWidget* showButton;

    /** Special widget to show  by hover. */
    QGraphicsWidget* showWidget;

    /** Hover processor that is used to show the  when the mouse hovers over it. */
    HoverFocusProcessor* showingProcessor;

    /** Hover processor that is used to hide the  when the mouse leaves it. */
    HoverFocusProcessor* hidingProcessor;

    AnimatorGroup* opacityAnimatorGroup;

    QTimer* autoHideTimer;

    qreal lastThumbnailsHeight;
};
