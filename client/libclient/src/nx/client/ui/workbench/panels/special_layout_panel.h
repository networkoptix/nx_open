#pragma once

#include <ui/workbench/panels/abstract_workbench_panel.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class SpecialLayoutPanelPrivate;
class SpecialLayoutPanel: public NxUi::AbstractWorkbenchPanel
{
    using base_type = NxUi::AbstractWorkbenchPanel;

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

    virtual void stopAnimations() override;

private:
    Q_DECLARE_PRIVATE(SpecialLayoutPanel);
    const QScopedPointer<SpecialLayoutPanelPrivate> d_ptr;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
