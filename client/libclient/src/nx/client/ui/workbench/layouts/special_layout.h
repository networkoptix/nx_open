#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/connective.h>

class QGraphicsWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

/**
 * @brief SpecialLayout class. Represents layout with autofilled background and top-anchored
 * workbench panel. Uses SpecialLayoutPanelWidget as default implementation for panel.
 */
class SpecialLayout: public Connective<QnWorkbenchLayout>
{
    Q_OBJECT
    using base_type = Connective<QnWorkbenchLayout>;

public:
    SpecialLayout(const QnLayoutResourcePtr& resource, QObject* parent = nullptr);
    virtual ~SpecialLayout();

    void setPanelWidget(QGraphicsWidget* widget); //< Takes ownership under widget
    QGraphicsWidget* panelWidget() const;

signals:
    void panelWidgetChanged();

private:
    QPointer<QGraphicsWidget> m_panelWidget;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
