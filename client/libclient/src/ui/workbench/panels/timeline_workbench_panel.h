#pragma once

#include <ui/workbench/panels/abstract_workbench_panel.h>

class QnNavigationItem;
class QnResizerWidget;
class QGraphicsWidget;
class HoverFocusProcessor;
class VariantAnimator;
class QnImageButtonWidget;
class AnimatorGroup;

namespace NxUi {

class TimelineWorkbenchPanel: public AbstractWorkbenchPanel
{
    using base_type = AbstractWorkbenchPanel;

    Q_OBJECT
public:
    TimelineWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent = nullptr);

    bool pinned;

    QnNavigationItem* item;

    bool zoomingIn;
    bool zoomingOut;

    /** Animator for  position. */
    VariantAnimator* yAnimator;

    QnImageButtonWidget* showButton;

    /** Special widget to show by hover. */
    QGraphicsWidget* showWidget;

    qreal lastThumbnailsHeight;

public:
    virtual bool isPinned() const override;

    virtual bool isOpened() const override;
    virtual void setOpened(bool opened = true, bool animate = true) override;

    virtual bool isVisible() const override;
    virtual void setVisible(bool visible = true, bool animate = true) override;

    virtual qreal opacity() const override;
    virtual void setOpacity(qreal opacity, bool animate = true) override;
    virtual void updateOpacity(bool animate) override;

    virtual bool isHovered() const override;

    bool isThumbnailsVisible() const;
    void setThumbnailsVisible(bool visible);

private:
    void setShowButtonUsed(bool used);
    void updateResizerGeometry();
    void updateControlsGeometry();

    qreal minimumHeight() const;

private:
    void at_resizerWidget_geometryChanged();
    void at_sliderResizerWidget_wheelEvent(QObject *target, QEvent *event);

private:
    bool m_visible;
    bool m_opened;

    /** We are currently in the resize process. */
    bool m_resizing;

    bool m_updateResizerGeometryLater;

    QnResizerWidget* m_resizerWidget;

    QGraphicsWidget* m_zoomButtonsWidget;

    QTimer* m_autoHideTimer;

    /** Hover processor that is used to hide the panel when the mouse leaves it. */
    HoverFocusProcessor* m_hidingProcessor;

    /** Hover processor that is used to show the panel when the mouse hovers over show button. */
    HoverFocusProcessor* m_showingProcessor;

    /** Hover processor that is used to change panel opacity when mouse hovers over it. */
    HoverFocusProcessor* m_opacityProcessor;

    /** Animator group for panel's opacity. */
    AnimatorGroup* m_opacityAnimatorGroup;
};

} //namespace NxUi
