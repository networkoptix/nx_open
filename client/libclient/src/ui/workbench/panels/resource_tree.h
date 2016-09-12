#pragma once

#include <ui/workbench/panels/abstract_workbench_panel.h>

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
    QnResizerWidget* resizerWidget;

    /** Proxy widget for navigation tree widget. */
    QnMaskedProxyWidget* item;

    /** Item that provides background for the tree. */
    QnControlBackgroundWidget* backgroundItem;

    /** Button to show/hide the tree. */
    QnImageButtonWidget* showButton;

    /** Button to pin the tree. */
    QnImageButtonWidget* pinButton;

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

    void updateResizerGeometry();

protected:
    void setProxyUpdatesEnabled(bool updatesEnabled) override;

private:
    void setShowButtonUsed(bool used);

private:
    void at_resizerWidget_geometryChanged();
    void at_showingProcessor_hoverEntered();

private:
    QGraphicsWidget* m_parentWidget;
    bool m_ignoreClickEvent;
    bool m_ignoreResizerGeometryChanges;
    bool m_updateResizerGeometryLater;
    bool m_visible;

    /** Hover processor that is used to hide the tree when the mouse leaves it. */
    HoverFocusProcessor* m_hidingProcessor;

    /** Hover processor that is used to show the tree when the mouse hovers over it. */
    HoverFocusProcessor* m_showingProcessor;

    /** Hover processor that is used to change tree opacity when mouse hovers over it. */
    HoverFocusProcessor* m_opacityProcessor;

    /** Animator group for tree's opacity. */
    AnimatorGroup* m_opacityAnimatorGroup;
};

}
