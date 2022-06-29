// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>

#include <nx/vms/client/core/network/remote_connection_aware.h>

namespace nx::vms::client::desktop::jsapi {

class Auth: public QObject, public core::RemoteConnectionAware
{
    Q_OBJECT
public:
    using AuthCondition = std::function<bool()>;

    Auth(AuthCondition authCondition, QObject* parent = nullptr);

    /** Returns a session token. */
    Q_INVOKABLE QString sessionToken() const;

    /** Returns a refresh token. */
    Q_INVOKABLE QString cloudToken() const;

private:
    AuthCondition m_checkCondition;
};

} // namespace nx::vms::client::desktop::jsapi
