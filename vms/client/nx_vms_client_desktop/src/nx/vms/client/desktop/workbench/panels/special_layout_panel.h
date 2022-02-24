// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_workbench_panel.h"

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

class SpecialLayoutPanelPrivate;
class SpecialLayoutPanel: public AbstractWorkbenchPanel
{
    using base_type = AbstractWorkbenchPanel;

public:
    SpecialLayoutPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent = nullptr);

    virtual ~SpecialLayoutPanel();

    QGraphicsWidget* widget();

public: // overrides
    virtual bool isPinned() const override;

    virtual bool isOpened() const override;
    virtual void setOpened(bool opened = true, bool animate = true) override;

    virtual bool isVisible() const override;
    virtual void setVisible(bool visible = true, bool animate = true) override;

    virtual qreal opacity() const override;
    virtual void setOpacity(qreal opacity, bool animate = true) override;

    virtual bool isHovered() const override;

    virtual QRectF effectiveGeometry() const override;
    virtual QRectF geometry() const override;

    virtual void stopAnimations() override;
    virtual void setPanelSize(qreal size) override;

private:
    Q_DECLARE_PRIVATE(SpecialLayoutPanel);
    const QScopedPointer<SpecialLayoutPanelPrivate> d_ptr;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
