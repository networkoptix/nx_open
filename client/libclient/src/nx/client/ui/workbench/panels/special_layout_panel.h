#pragma once

#include <ui/workbench/panels/abstract_workbench_panel.h>
#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace panels {

class SpecialLayoutPanel: public Connective<NxUi::AbstractWorkbenchPanel>
{
    using base_type = Connective<NxUi::AbstractWorkbenchPanel>;

public:
    SpecialLayoutPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent = nullptr);

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
    class PanelPrivate;
    const QScopedPointer<PanelPrivate> d;
};

} // namespace panels
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx
