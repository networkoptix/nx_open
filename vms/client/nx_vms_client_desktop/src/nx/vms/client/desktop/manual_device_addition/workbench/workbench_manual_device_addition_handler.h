// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class DeviceAdditionDialog;
class NewDeviceAdditionDialog;

class WorkbenchManualDeviceAdditionHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnWorkbenchContextAware;

public:
    WorkbenchManualDeviceAdditionHandler(QObject* parent = nullptr);

private:
    QPointer<DeviceAdditionDialog> m_deviceAdditionDialog;
    QPointer<NewDeviceAdditionDialog> m_newDeviceAdditionDialog;
};

} // namespace nx::vms::client::desktop
