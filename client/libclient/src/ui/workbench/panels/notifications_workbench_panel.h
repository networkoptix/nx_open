#pragma once

#include <ui/workbench/panels/abstract_workbench_panel.h>

class QnControlBackgroundWidget;
class QnNotificationsCollectionWidget;
class QnImageButtonWidget;
class QnBlinkingImageButtonWidget;
class HoverFocusProcessor;
class AnimatorGroup;
class VariantAnimator;

namespace NxUi {

class NotificationsWorkbenchPanel: public AbstractWorkbenchPanel
{
    using base_type = AbstractWorkbenchPanel;

    Q_OBJECT
public:
    NotificationsWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent = nullptr);

    QnControlBackgroundWidget* backgroundItem;
    QnNotificationsCollectionWidget* item;
    QnImageButtonWidget* pinButton;
    QnBlinkingImageButtonWidget* showButton;
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

private:
    void setShowButtonUsed(bool used);
    void at_item_geometryChanged();

private:
    void at_showingProcessor_hoverEntered();

private:
    bool m_ignoreClickEvent;
    bool m_visible;

    /** Hover processor that is used to hide the panel when the mouse leaves it. */
    HoverFocusProcessor* m_hidingProcessor;

    /** Hover processor that is used to show the panel when the mouse hovers over show button. */
    HoverFocusProcessor* m_showingProcessor;

    /** Hover processor that is used to change panel opacity when mouse hovers over it. */
    HoverFocusProcessor* m_opacityProcessor;

    /** Animator group for panel's opacity. */
    AnimatorGroup* m_opacityAnimatorGroup;
};

}
