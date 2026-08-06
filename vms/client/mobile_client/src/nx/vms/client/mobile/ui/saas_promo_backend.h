// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/scoped_connections.h>

Q_MOC_INCLUDE("nx/vms/client/core/system_context.h")

namespace nx::vms::client::core { class SystemContext; }

namespace nx::vms::client::mobile {

class SaasPromoBackend: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(nx::vms::client::core::SystemContext* systemContext READ systemContext WRITE
            setSystemContext NOTIFY systemContextChanged)

public:
    SaasPromoBackend(QObject* parent = nullptr);

    bool available() const { return m_available; }

    core::SystemContext* systemContext() const { return m_context; }
    void setSystemContext(core::SystemContext* context);

    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void openPromoUrl();

    static void registerQmlType();

signals:
    void systemContextChanged();
    void availableChanged();

private:
    void updateAvailability();

private:
    bool m_available = false;
    QPointer<core::SystemContext> m_context;
    nx::utils::ScopedConnections m_contextConnections;
};

} // namespace nx::vms::client::mobile
