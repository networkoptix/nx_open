// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/action_types.h>

#include "abstract_workbench_panel.h"

class QnResizerWidget;
class QnControlBackgroundWidget;
class QnImageButtonWidget;
class HoverFocusProcessor;
class AnimatorGroup;
class VariantAnimator;

namespace nx::vms::client::desktop {

class LeftPanelWidget;

class LeftWorkbenchPanel: public AbstractWorkbenchPanel
{
    using base_type = AbstractWorkbenchPanel;
    Q_OBJECT

public:
    LeftWorkbenchPanel(
        const QnPaneSettings& settings,
        QWidget* parentWidget,
        QGraphicsWidget* parentGraphicsWidget,
        QObject* parent = nullptr);

public:
    void setYAndHeight(int y, int height);
    void setMaximumAllowedWidth(qreal value);

    virtual bool isPinned() const override;

    virtual bool isOpened() const override;
    virtual void setOpened(bool opened = true, bool animate = true) override;

    virtual bool isVisible() const override;
    virtual void setVisible(bool visible = true, bool animate = true) override;

    virtual qreal opacity() const override;
    virtual void setOpacity(qreal opacity, bool animate = true) override;

    virtual bool isHovered() const override;
    virtual bool isFocused() const override;

    virtual QRectF effectiveGeometry() const override;
    virtual QRectF geometry() const override;

    virtual void stopAnimations() override;
    virtual void setPanelSize(qreal /*size*/) override {}

    menu::ActionScope currentScope() const;
    menu::Parameters currentParameters(menu::ActionScope scope) const;

private:
    void updateControlsGeometry();
    void updateMaximumWidth();
    int effectiveWidth() const;

private:
    LeftPanelWidget* const widget;

    bool m_visible = false;
    bool m_blockAction = false;

    int m_maximumAllowedWidth = 0;

    QnResizerWidget* m_resizerWidget;

    /** Animator for tree's position. */
    VariantAnimator* xAnimator;

    /** Item that provides background for the tree. */
    QnControlBackgroundWidget* m_backgroundItem;

    /** Animator group for panel's opacity. */
    AnimatorGroup* m_opacityAnimatorGroup;
};

} //namespace nx::vms::client::desktop
