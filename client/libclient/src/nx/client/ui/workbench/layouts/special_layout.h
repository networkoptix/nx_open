#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_layout.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class SpecialLayout: public QnWorkbenchLayout
{
    Q_OBJECT
    using base_type = QnWorkbenchLayout;

public:
    SpecialLayout(const QnLayoutResourcePtr& resource, QObject* parent = nullptr);
    virtual ~SpecialLayout();

    void setPanelCaption(const QString& caption);

public: // overrides
    virtual QnWorkbenchLayout::GraphicsWidgetPtr createPanelWidget() const override;

private:
    class SpecialLayoutPrivate;
    QScopedPointer<SpecialLayoutPrivate> d;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
