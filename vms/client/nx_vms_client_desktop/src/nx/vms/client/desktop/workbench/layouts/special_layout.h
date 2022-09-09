// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <ui/workbench/workbench_layout.h>

class QGraphicsWidget;

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

/**
 * @brief SpecialLayout class. Represents layout with autofilled background and top-anchored
 * workbench panel. Uses SpecialLayoutPanelWidget as default implementation for panel.
 */
class SpecialLayout: public QnWorkbenchLayout
{
    Q_OBJECT
    using base_type = QnWorkbenchLayout;

public:
    SpecialLayout(const LayoutResourcePtr& resource);
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
} // namespace nx::vms::client::desktop
