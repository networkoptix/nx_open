// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth.h"

#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/common/system_settings.h>

#include "types.h"

namespace nx::vms::client::desktop::jsapi {

Auth::Auth(SystemContext* systemContext,AuthCondition authCondition, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext),
    m_checkCondition(authCondition)
{
    registerTypes();
}

QString Auth::sessionToken() const
{
    if (!NX_ASSERT(m_checkCondition))
        return {};

    const auto token = connection()->credentials().authToken;
    if (token.isBearerToken() && m_checkCondition())
        return QString::fromStdString(token.value);

    return {};
}

QString Auth::cloudSystemId() const
{
    return systemSettings()->cloudSystemId();
}

QString Auth::cloudToken() const
{
    if (!NX_ASSERT(m_checkCondition))
        return {};

    if (m_checkCondition())
    {
        return QString::fromStdString(
            qnCloudStatusWatcher->remoteConnectionCredentials().authToken.value);
    }

    return {};
}

QString Auth::cloudHost() const
{
    return systemSettings()->cloudHost();
}

} // namespace nx::vms::client::desktop::jsapi
