// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_workbench_panel.h"

#include <QtCore/QScopedPointer>
#include <QtCore/QRect>
#include <QtWidgets/QWidget>

class QnControlBackgroundWidget;
class QnBlinkingImageButtonWidget;
class HoverFocusProcessor;
class AnimatorGroup;
class VariantAnimator;

namespace nx::vms::client::desktop {

class EventPanel;
class MultiImageProvider;

class BaseWidget : public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

    Q_PROPERTY(int x READ x WRITE setX NOTIFY xChanged)

public:
    BaseWidget(QWidget* parent = nullptr);
    ~BaseWidget();

    int x() const;
    void setX(int x);

signals:
    void xChanged(int newX);
    void geometryChanged();
};

class NotificationsWorkbenchPanel: public AbstractWorkbenchPanel
{
    Q_OBJECT
    using base_type = AbstractWorkbenchPanel;

public:
    NotificationsWorkbenchPanel(
        const QnPaneSettings& settings,
        QWidget* parentWidget,
        QGraphicsWidget* parentGraphicsWidget,
        QObject* parent = nullptr);

    virtual ~NotificationsWorkbenchPanel() override;

public:
    void setX(qreal x);
    void setPos(const QPoint& pos);
    void resize(const QSize& size);

    virtual bool isPinned() const override;

    virtual bool isOpened() const override;
    virtual void setOpened(bool opened = true, bool animate = true) override;

    virtual bool isVisible() const override;
    virtual void setVisible(bool visible = true, bool animate = true) override;

    virtual qreal opacity() const override;
    virtual void setOpacity(qreal opacity, bool animate = true) override;

    virtual bool isHovered() const override;

    virtual void setPanelSize(qreal size) override;
    virtual QRectF effectiveGeometry() const override;
    virtual QRectF geometry() const override;

    virtual void stopAnimations() override;

    bool resizeable() const;
    void setResizeable(bool value);

    BaseWidget *baseWidget() const {
        return m_baseWidget;
    }

private:
    void enableShowButton(bool used);
    void updateControlsGeometry();
    void initEventPanel();

private:
    bool m_visible = false;
    bool m_blockAction = false;
    bool m_resizeable = false;

    QnControlBackgroundWidget* const backgroundItem;
    VariantAnimator* const xAnimator;

    QnBlinkingImageButtonWidget* m_showButton;

    /** Animator group for panel's opacity. */
    AnimatorGroup* m_opacityAnimatorGroup;

    // We cannot use EventPanel as a base widget, because it will be hidden sometimes,
    // and when it hidden, it doesn't handle Move and Resize events;
    BaseWidget* m_baseWidget;

    /** New event panel. */
    EventPanel* m_widget;
    QPointer<HoverFocusProcessor> m_eventPanelHoverProcessor;

    class ResizerWidget;
    QScopedPointer<ResizerWidget> m_panelResizer;
};

} //namespace nx::vms::client::desktop
