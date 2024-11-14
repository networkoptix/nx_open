// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <api/model/api_ioport_data.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API IOModuleMonitor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    IOModuleMonitor(const QnVirtualCameraResourcePtr &camera, QObject* parent = nullptr);
    virtual ~IOModuleMonitor() override;

    bool open();
    bool connectionIsOpened() const;

signals:
    void connectionClosed();
    void connectionOpened();
    void ioStateChanged(const QnIOStateData& value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

using IOModuleMonitorPtr = QSharedPointer<IOModuleMonitor>;

} // namespace nx::vms::client::core
