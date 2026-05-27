// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fresh_session_token_helper.h"

#include <QtCore/QThread>
#include <QtWidgets/QApplication>

#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/oauth_client_constants.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/connect_actions_handler.h>
#include <nx/vms/client/desktop/system_logon/ui/oauth_login_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_dialog.h>
#include <nx/vms/client/desktop/window_context.h>

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
            return {core::OauthClientType::passwordApply};
        case ActionType::issueRefreshToken:
            return {core::OauthClientType::renew};
    }

    NX_ASSERT(false, "Unexpected action type: %1", (int) actionType);
    return {core::OauthClientType::undefined};
}

} // namespace.

FreshSessionTokenHelper::FreshSessionTokenHelper(
    QWidget* parent, const QString& title, ActionType action)
    :
    m_parent(parent),
    m_title(title),
    m_actionType(action)
{
    NX_CRITICAL(m_parent);
}

FreshSessionTokenHelper::~FreshSessionTokenHelper()
{
}

common::SessionTokenHelperPtr FreshSessionTokenHelper::makeHelper(
    QWidget* parent,
    const QString& title,
    const QString& mainText,
    const QString& actionText,
    ActionType actionType)
{
    auto result = new FreshSessionTokenHelper(parent, title, actionType);
    result->m_mainText = mainText;
    result->m_actionText = actionText;
    return common::SessionTokenHelperPtr(result);
}

void FreshSessionTokenHelper::setFlags(SessionRefreshFlags flags)
{
    m_flags = flags;
}

std::optional<nx::network::http::AuthToken> FreshSessionTokenHelper::refreshSession()
{
    m_password = {};

    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return {};

    if (!m_parent)
        return {};

    nx::network::http::AuthToken token;
    // TODO: #amalov Make FreshSessionTokenHelper windows context aware.
    const auto windowContext = appContext()->mainWindowContext();
    const auto connection = windowContext->system()->connection();
    if (!connection)
        return {};

    if (connection->userType() == nx::vms::api::UserType::cloud)
    {
        const auto params = OauthLoginDialog::LoginParams{
            .clientType = info(m_actionType).clientType,
            .flags = m_flags | SessionRefreshFlag::sessionAware | SessionRefreshFlag::stayOnTop,
            .cloudSystem = connection->moduleInformation().cloudSystemId
        };

        auto cloudAuthData = OauthLoginDialog::login(m_parent, m_title, params);

        if (!cloudAuthData)
        {
            NX_WARNING(this, "Cloud login failed due to %1", cloudAuthData.error());
            return {};
        }

        if (cloudAuthData->needValidateToken && !OauthLoginDialog::validateToken(
                m_parent, m_title, cloudAuthData->credentials))
        {
            return {};
        }

        cloudAuthData->credentials.username = qnCloudStatusWatcher->credentials().username;
        qnCloudStatusWatcher->setAuthData(
            *cloudAuthData, core::CloudStatusWatcher::AuthMode::update);

        windowContext->connectActionsHandler()->updateCloudSessionToken();

        token = cloudAuthData->credentials.authToken;
    }
    else
    {
        const auto result = SessionRefreshDialog::refreshSession(
            m_parent,
            m_title,
            m_mainText,
            /*infoText*/ QString(),
            m_actionText,
            info(m_actionType).isImportant,
            /*passwordValidationMode*/ false,
            Qt::WindowStaysOnTopHint
        );

        token = result.token;
        if (!token.empty())
            m_password = result.password;
    }
    NX_ASSERT(token.empty() || token.isBearerToken());

    if (token.empty())
        return {};

    return token;
}

core::CloudAuthDataOrError
    FreshSessionTokenHelper::requestAuthData(std::function<bool()> closeCondition) const
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return std::unexpected{core::OauthDataRequestError::wrongThread};

    if (!m_parent)
        return std::unexpected{core::OauthDataRequestError::noParent};

    const auto clientType = info(m_actionType).clientType;
    if (!NX_ASSERT(clientType == core::OauthClientType::renew))
        return std::unexpected{core::OauthDataRequestError::unexpectedClientType};

    auto credentials = qnCloudStatusWatcher->credentials();
    if (credentials.username.empty())
        return std::unexpected{core::OauthDataRequestError::cloudLoginEmpty};

    const auto params = OauthLoginDialog::LoginParams{
        .clientType = clientType,
        .flags = m_flags,
        .credentials = credentials,
        .closeCondition = closeCondition
    };

    const auto cloudAuthData = OauthLoginDialog::login(m_parent, m_title, params);

    if (!cloudAuthData)
        NX_WARNING(this, "Cloud login failed due to %1", cloudAuthData.error());

    if (cloudAuthData && cloudAuthData->needValidateToken
        && !OauthLoginDialog::validateToken(m_parent, m_title, cloudAuthData->credentials))
    {
        return std::unexpected{core::OauthDataRequestError::tokenValidationFailed};
    }

    return cloudAuthData;
}

QString FreshSessionTokenHelper::password() const
{
    return m_password;
}

} // namespace nx::vms::client::desktop
