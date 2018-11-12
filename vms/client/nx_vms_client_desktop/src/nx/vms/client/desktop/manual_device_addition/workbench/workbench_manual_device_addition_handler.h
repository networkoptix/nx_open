#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class DeviceAdditionDialog;

class WorkbenchManualDeviceAdditionHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnWorkbenchContextAware;

public:
    WorkbenchManualDeviceAdditionHandler(QObject* parent = nullptr);

private:
    QPointer<DeviceAdditionDialog> m_deviceAdditionDialog;
};

} // namespace nx::vms::client::desktop
