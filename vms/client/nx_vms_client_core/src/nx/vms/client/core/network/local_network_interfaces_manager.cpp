// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_network_interfaces_manager.h"

#include <QtCore/QTimer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkInterface>

template<>
nx::vms::client::core::LocalNetworkInterfacesManager*
    Singleton<nx::vms::client::core::LocalNetworkInterfacesManager>::s_instance = nullptr;

namespace nx::vms::client::core {

struct LocalNetworkInterfacesManager::Private
{
    QTimer timer;
    QSet<QString> localAddresses;

    void start()
    {
        timer.setInterval(std::chrono::minutes(1));
        timer.start();
    }

    void update()
    {
        auto addresses = QNetworkInterface::allAddresses();
        if (!addresses.empty())
        {
            localAddresses.clear();
            for (const auto& localAddress: addresses)
                localAddresses.insert(localAddress.toString());
        }
    }
};

LocalNetworkInterfacesManager::LocalNetworkInterfacesManager(QObject* parent):
    base_type(parent),
    d(new Private())
{
    QObject::connect(&d->timer, &QTimer::timeout, [this]{ d->update(); });

    d->update();
    d->start();
}

LocalNetworkInterfacesManager::~LocalNetworkInterfacesManager()
{
}

bool LocalNetworkInterfacesManager::containsHost(const QString& host) const
{
    return d->localAddresses.contains(host);
}

} // namespace nx::vms::client::core
