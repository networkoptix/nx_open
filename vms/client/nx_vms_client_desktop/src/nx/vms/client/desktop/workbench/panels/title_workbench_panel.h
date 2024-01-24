// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/workbench/panels/abstract_workbench_panel.h>

class QnImageButtonWidget;
class HoverFocusProcessor;
class VariantAnimator;
class QnMainWindowTitleBarWidget;

namespace nx::vms::client::desktop {

class TitleWorkbenchPanel: public AbstractWorkbenchPanel
{
    using base_type = AbstractWorkbenchPanel;

    Q_OBJECT
public:
    TitleWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentGraphicsWidget,
        QObject* parent = nullptr);

public:
    virtual bool isPinned() const override;

    virtual bool isOpened() const override;
    virtual void setOpened(bool opened = true, bool animate = true) override;

    virtual bool isVisible() const override;
    virtual void setVisible(bool visible = true, bool animate = true) override;

    virtual qreal opacity() const override;
    virtual void setOpacity(qreal opacity, bool animate = true) override;

    virtual bool isHovered() const override;

    virtual void stopAnimations() override;

    virtual QRectF effectiveGeometry() const override;
    virtual void setPanelSize(qreal size) override;

    void setGeometry(const QRect& geometry);
    virtual QRectF geometry() const override;

    QSize sizeHint() const;
    void activatePreviousSystemTab();

private:
    void updateControlsGeometry();

private:
    bool m_ignoreClickEvent = false;

    QnMainWindowTitleBarWidget* m_widget;

    QPointer<QnImageButtonWidget> m_showButton;

    /** Animator for title's position. */
    QPointer<VariantAnimator> m_yAnimator;
    QPointer<HoverFocusProcessor> m_opacityProcessor;
};

} //namespace nx::vms::client::desktop
