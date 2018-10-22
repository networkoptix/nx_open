#pragma once

#include <QtCore/QScopedPointer>

#include <ui/workbench/panels/abstract_workbench_panel.h>

class QnControlBackgroundWidget;
class QnNotificationToolTipWidget;
class QnImageButtonWidget;
class QnBlinkingImageButtonWidget;
class HoverFocusProcessor;
class AnimatorGroup;
class VariantAnimator;

namespace nx {
namespace client {
namespace desktop {

class EventPanel;
class EventTileToolTip;
class EventTile;

} // namespace desktop
} // namespace client
} // namespace nx

namespace NxUi {

class NotificationsWorkbenchPanel: public AbstractWorkbenchPanel
{
    Q_OBJECT
    using base_type = AbstractWorkbenchPanel;

public:
    NotificationsWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent = nullptr);

    virtual ~NotificationsWorkbenchPanel() override;

    QnControlBackgroundWidget* const backgroundItem;
    QGraphicsWidget* const item;
    VariantAnimator* const xAnimator;
    QRectF tooltipsEnclosingRect;

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

private:
    void enableShowButton(bool used);
    void setShowButtonIcon();
    void updateControlsGeometry();

private:
    void at_showingProcessor_hoverEntered();

    void createEventPanel(QGraphicsWidget* parentWidget);

    void at_eventTileHovered(const QModelIndex& index, const nx::client::desktop::EventTile* tile);

private:
    bool m_visible = false;
    bool m_opened = false;

    QnBlinkingImageButtonWidget* m_showButton;

    /** Hover processor that is used to hide the panel when the mouse leaves it. */
    HoverFocusProcessor* m_hidingProcessor;

    /** Hover processor that is used to show the panel when the mouse hovers over show button. */
    HoverFocusProcessor* m_showingProcessor;

    /** Hover processor that is used to change panel opacity when mouse hovers over it. */
    HoverFocusProcessor* m_opacityProcessor;

    /** Animator group for panel's opacity. */
    AnimatorGroup* m_opacityAnimatorGroup;

    /** New event panel. */
    QGraphicsProxyWidget* m_eventPanelContainer = nullptr;
    QScopedPointer<nx::client::desktop::EventPanel> m_eventPanel;
    QPointer<HoverFocusProcessor> m_eventPanelHoverProcessor;
    const nx::client::desktop::EventTile* m_lastHoveredTile = nullptr;
};

} //namespace NxUi
