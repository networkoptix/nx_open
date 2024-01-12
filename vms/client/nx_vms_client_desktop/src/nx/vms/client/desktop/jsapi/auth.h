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

    /**
     * @addtogroup vms-auth
     * This section contains specification for the authentication API.
     *
     * Contains methods to retrieve authentication data.
     *
     * Example:
     *
     *     const token = await window.vms.auth.sessionToken();
     *
     * @{
     */

    /**
     * @return A user session token or an empty string if it is inaccessible (e.g. the user
     * prohibited the token transfer). Could be used to authorize the Rest API calls to the
     * currently connected System. If the Cloud User is not part of the System, or if the System is
     * not connected to the Cloud at all, a local bearer token will be returned.
     */
    Q_INVOKABLE QString sessionToken() const;

    /**
     * @return The Cloud System id or an empty string if the System is not connected to the Cloud.
     */
    Q_INVOKABLE QString cloudSystemId() const;

    /**
     * @return A Cloud refresh token or an empty string if it is inaccessible (e.g. the user
     * prohibited token transfer). This token can be used to obtain an access token (using the
     * Cloud) with the specified scope.
     * - Returns the token even if the Cloud User is not part of the currently connected System
     *     (local user login).
     * - Returns nothing with no error message if the Client is not logged into the Cloud.
     */
    Q_INVOKABLE QString cloudToken() const;

    /** @return The cloud host. */
    Q_INVOKABLE QString cloudHost() const;

    /** @} */ // group vms-auth

private:
    AuthCondition m_checkCondition;
};

} // namespace nx::vms::client::desktop::jsapi
