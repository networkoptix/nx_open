#pragma once

#include <QtCore/QScopedPointer>

#include "abstract_workbench_panel.h"

class QGraphicsOpacityEffect;
class QnControlBackgroundWidget;
class QnNotificationToolTipWidget;
class QnImageButtonWidget;
class QnBlinkingImageButtonWidget;
class HoverFocusProcessor;
class AnimatorGroup;
class VariantAnimator;

namespace nx::vms::client::desktop {

class EventPanel;
class EventTileToolTip;
class EventTile;
class MainWindow;

class NotificationsWorkbenchPanel: public AbstractWorkbenchPanel
{
    Q_OBJECT
    using base_type = AbstractWorkbenchPanel;

public:
    NotificationsWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        MainWindow* mainWindow,
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
    void updateControlsGeometry();

private:
    void at_showingProcessor_hoverEntered();

    void createEventPanel(QGraphicsWidget* parentWidget, MainWindow* mainWindow);

    void at_eventTileHovered(const QModelIndex& index, EventTile* tile);

private:
    bool m_visible = false;
    bool m_blockAction = false;

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
    QGraphicsWidget* m_eventPanelPlacement = nullptr;
    QScopedPointer<EventPanel> m_eventPanel;
    QPointer<HoverFocusProcessor> m_eventPanelHoverProcessor;
    QPointer<QGraphicsOpacityEffect> m_eventPanelOpacityEffect;

    // Required to correctly display tooltip.
    const EventTile* m_lastHoveredTile = nullptr;
};

} //namespace nx::vms::client::desktop
