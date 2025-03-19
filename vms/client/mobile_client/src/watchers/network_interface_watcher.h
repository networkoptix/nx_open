// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/context_from_qml_handler.h>
#include <nx/vms/client/mobile/window_context_aware.h>

class QnResourceDiscoveryManager;

namespace nx::vms::client::mobile {

class NetworkInterfaceWatcher: public QObject,
    public WindowContextAware,
    public core::ContextFromQmlHandler
{
    Q_OBJECT

public:
    NetworkInterfaceWatcher(QObject* parent = nullptr);


signals:
    void interfacesChanged();

private:
    virtual void onContextReady() override;

private:
    QPointer<QnResourceDiscoveryManager> m_discoveryManager;
};

} // namespace nx::vms::client::mobile
