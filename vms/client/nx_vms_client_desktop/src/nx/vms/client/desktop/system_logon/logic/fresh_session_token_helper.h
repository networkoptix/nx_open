// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/auth_tools.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

namespace nx::vms::client::desktop {

/**
* Displays password confirmation dialog for local user, or cloud OAuth dialog for cloud.
*
* Intended to use with server owner API calls
*/
class FreshSessionTokenHelper: public nx::vms::client::core::RemoteConnectionAware
{
public:
    enum class ActionType
    {
        merge,
        unbind,
        backup,
        restore,
        updateSettings,
    };

public:
    FreshSessionTokenHelper(QWidget* parent);

    nx::network::http::AuthToken getToken(
        const QString& title,
        const QString& mainText,
        const QString& actionText, //< Action button text.
        ActionType actionType);

private:
    QWidget* m_parent = nullptr;
};

} // namespace nx::vms::client::desktop
