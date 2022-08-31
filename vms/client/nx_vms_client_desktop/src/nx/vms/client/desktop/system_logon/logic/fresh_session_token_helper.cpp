// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fresh_session_token_helper.h"

#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/oauth_client_constants.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/system_logon/ui/oauth_login_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_dialog.h>

namespace nx::vms::client::desktop {
namespace {

struct ActionTypeInfo
{
    core::OauthClientType clientType;
    bool isImportant = true;
};

ActionTypeInfo info(FreshSessionTokenHelper::ActionType actionType)
{
    using ActionType = FreshSessionTokenHelper::ActionType;
    switch (actionType)
    {
        case ActionType::unbind:
            return {core::OauthClientType::passwordDisconnect};
        case ActionType::backup:
            return {core::OauthClientType::passwordBackup, /* isImportant*/ false};
        case ActionType::restore:
            return {core::OauthClientType::passwordRestore};
        case ActionType::merge:
            return {core::OauthClientType::passwordMerge};
        case ActionType::updateSettings:
            return {core::OauthClientType::renewDesktop};
    }

    NX_ASSERT(false, "Unexpected action type");
    return {core::OauthClientType::undefined};
}

} // namespace.

FreshSessionTokenHelper::FreshSessionTokenHelper(QWidget* parent):
    m_parent(parent)
{
    NX_CRITICAL(m_parent);
}

nx::network::http::AuthToken FreshSessionTokenHelper::getToken(
    const QString& title,
    const QString& mainText,
    const QString& actionText,
    ActionType actionType)
{
    nx::network::http::AuthToken token;
    auto connection = this->connection();
    if (!connection)
        return token;

    if (connection->userType() == nx::vms::api::UserType::cloud)
    {
        token = OauthLoginDialog::login(
            m_parent,
            title,
            info(actionType).clientType,
            /*sessionAware*/ true,
            connection->moduleInformation().cloudSystemId
        ).credentials.authToken;
    }
    else
    {
        token = SessionRefreshDialog::refreshSession(
            m_parent,
            title,
            mainText,
            /*infoText*/ QString(),
            actionText,
            info(actionType).isImportant,
            /*passwordValidationMode*/ false
        ).token;
    }

    NX_ASSERT(token.empty() || token.isBearerToken());
    return token;
}

} // namespace nx::vms::client::desktop
