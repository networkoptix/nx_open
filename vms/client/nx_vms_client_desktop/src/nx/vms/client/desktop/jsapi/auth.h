// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>

#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop::jsapi {

class Auth:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
public:
    using AuthCondition = std::function<bool()>;

    Auth(SystemContext* systemContext, AuthCondition authCondition, QObject* parent = nullptr);

    /** Returns the session token. */
    Q_INVOKABLE QString sessionToken() const;

    /** Returns the cloud system id. */
    Q_INVOKABLE QString cloudSystemId() const;

    /** Returns the refresh token. */
    Q_INVOKABLE QString cloudToken() const;

    /** Returns the cloud host. */
    Q_INVOKABLE QString cloudHost() const;

private:
    AuthCondition m_checkCondition;
};

} // namespace nx::vms::client::desktop::jsapi
