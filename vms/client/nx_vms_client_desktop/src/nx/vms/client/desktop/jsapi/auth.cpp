// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth.h"

#include <nx/vms/client/core/network/remote_connection.h>

namespace nx::vms::client::desktop::jsapi {

Auth::Auth(AuthCondition authCondition, QObject* parent):
    QObject(parent),
    m_checkCondition(authCondition)
{
}

QString Auth::sessionToken() const
{
    if (!NX_ASSERT(m_checkCondition))
        return {};

    const auto token = connectionCredentials().authToken;
    if (token.isBearerToken() && m_checkCondition())
        return QString::fromStdString(token.value);

    return {};
}

} // namespace nx::vms::client::desktop::jsapi
