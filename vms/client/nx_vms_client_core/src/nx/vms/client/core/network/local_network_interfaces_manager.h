// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/singleton.h>

namespace nx::vms::client::core {

class LocalNetworkInterfacesManager: public QObject, public Singleton<LocalNetworkInterfacesManager>
{
    Q_OBJECT
    typedef QObject base_type;

public:
    LocalNetworkInterfacesManager(QObject* parent = nullptr);
    virtual ~LocalNetworkInterfacesManager() override;

    bool containsHost(const QString& host) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core

#define qnLocalNetworkInterfacesManager nx::vms::client::core::LocalNetworkInterfacesManager::instance()
