// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_workbench_panel.h"

class QnResizerWidget;
class QnControlBackgroundWidget;
class QnImageButtonWidget;
class HoverFocusProcessor;
class AnimatorGroup;
class VariantAnimator;

namespace nx::vms::client::desktop {

class QmlResourceBrowserWidget;

class ResourceTreeWorkbenchPanel: public AbstractWorkbenchPanel
{
    Q_OBJECT
    using base_type = AbstractWorkbenchPanel;

public:
    ResourceTreeWorkbenchPanel(
        const QnPaneSettings& settings,
        QWidget* parentWidget,
        QGraphicsWidget* parentGraphicsWidget,
        QObject* parent = nullptr);

    QmlResourceBrowserWidget* const widget;

public:
    void setYAndHeight(int y, int height);

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
    virtual void setPanelSize(qreal size) override;

private:
    void setShowButtonUsed(bool used);
    void updateResizerGeometry();
    void updateControlsGeometry();
    void updatePaneWidth(qreal desiredWidth);

private:
    bool m_visible = false;
    bool m_blockAction = false;

    bool m_updateResizerGeometryLater = false;

    bool m_inSelection = false;
    bool m_resizeInProgress = false;

    QnResizerWidget* m_resizerWidget;

    /** Animator for tree's position. */
    VariantAnimator* xAnimator;

    /** Item that provides background for the tree. */
    QnControlBackgroundWidget* m_backgroundItem;

    /** Button to show/hide the tree. */
    QnImageButtonWidget* m_showButton;

    /** Animator group for panel's opacity. */
    AnimatorGroup* m_opacityAnimatorGroup;
};

} //namespace nx::vms::client::desktop
