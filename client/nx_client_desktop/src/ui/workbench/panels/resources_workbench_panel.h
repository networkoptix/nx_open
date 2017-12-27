#pragma once

#include <ui/workbench/panels/abstract_workbench_panel.h>

class QnResourceBrowserWidget;
class QnResizerWidget;
class QnMaskedProxyWidget;
class QnControlBackgroundWidget;
class QnImageButtonWidget;
class HoverFocusProcessor;
class AnimatorGroup;
class VariantAnimator;

namespace NxUi {

class ResourceTreeWorkbenchPanel: public AbstractWorkbenchPanel
{
    using base_type = AbstractWorkbenchPanel;

    Q_OBJECT
public:
    ResourceTreeWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent = nullptr);

    /** Navigation tree widget. */
    QnResourceBrowserWidget* widget;

    /** Proxy widget for navigation tree widget. */
    QnMaskedProxyWidget* item;

    /** Animator for tree's position. */
    VariantAnimator* xAnimator;

public:
    virtual bool isPinned() const override;

    virtual bool isOpened() const override;
    virtual void setOpened(bool opened = true, bool animate = true) override;

    virtual bool isVisible() const override;
    virtual void setVisible(bool visible = true, bool animate = true) override;

    virtual qreal opacity() const override;
    virtual void setOpacity(qreal opacity, bool animate = true) override;

    virtual bool isHovered() const override;

    virtual QRectF effectiveGeometry() const override;

    virtual void stopAnimations() override;
protected:
    void setProxyUpdatesEnabled(bool updatesEnabled) override;

private:
    void setShowButtonUsed(bool used);
    void updateResizerGeometry();
    void updateControlsGeometry();
    void updatePaneWidth(qreal desiredWidth);

private:
    void at_resizerWidget_geometryChanged();
    void at_showingProcessor_hoverEntered();
    void at_selectNewItemAction_triggered();

private:
    bool m_ignoreClickEvent;
    bool m_visible;

    /** We are currently in the resize process. */
    bool m_resizing;
    bool m_updateResizerGeometryLater;

    bool m_inSelection = false;

    QnResizerWidget* m_resizerWidget;

    /** Item that provides background for the tree. */
    QnControlBackgroundWidget* m_backgroundItem;

    /** Button to show/hide the tree. */
    QnImageButtonWidget* m_showButton;

    /** Button to pin the tree. */
    QnImageButtonWidget* m_pinButton;

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
