// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fresh_session_token_helper.h"

#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/system_logon/ui/oauth_login_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_dialog.h>

namespace nx::vms::client::desktop {
namespace {

struct ActionTypeInfo
{
    QString clientType;
    bool isImportant;
};

ActionTypeInfo info(FreshSessionTokenHelper::ActionType actionType)
{
    using ActionType = FreshSessionTokenHelper::ActionType;
    switch (actionType)
    {
        case ActionType::unbind: return {.clientType="passwordDisconnect", .isImportant=true};
        case ActionType::backup: return {.clientType="passwordBackup", .isImportant=false};
        case ActionType::restore: return {.clientType="passwordRestore", .isImportant=true};
        case ActionType::merge: return {.clientType="passwordMerge", .isImportant=true};
    }

    NX_ASSERT(false, "Unexpected action type");
    return {.clientType = "", .isImportant = true};
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
