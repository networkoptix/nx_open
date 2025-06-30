// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fresh_session_token_helper.h"

#include <QtCore/QThread>
#include <QtWidgets/QApplication>

#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/oauth_client_constants.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
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
    const auto context = appContext()->currentSystemContext();
    auto connection = context->connection();
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

        if (cloudAuthData.empty() || (cloudAuthData.needValidateToken
            && !OauthLoginDialog::validateToken(m_parent, m_title, cloudAuthData.credentials)))
        {
            return {};
        }

        cloudAuthData.credentials.username = qnCloudStatusWatcher->credentials().username;
        qnCloudStatusWatcher->setAuthData(cloudAuthData,
            core::CloudStatusWatcher::AuthMode::update);
        if (auto session = context->session(); session && session->connection())
            session->updateCloudSessionToken();

        token = cloudAuthData.credentials.authToken;
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

core::CloudAuthData FreshSessionTokenHelper::requestAuthData(std::function<bool()> closeCondition)
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return {};

    if (!m_parent)
        return {};

    const auto login = qnCloudStatusWatcher->cloudLogin();
    if (login.isEmpty())
        return {};

    const auto params = OauthLoginDialog::LoginParams{
        .clientType = info(m_actionType).clientType,
        .flags = m_flags,
        .credentials = nx::network::http::Credentials(login.toStdString(), {}),
        .closeCondition = closeCondition
    };

    const auto cloudAuthData = OauthLoginDialog::login(m_parent, m_title, params);

    if (cloudAuthData.needValidateToken
        && !OauthLoginDialog::validateToken(m_parent, m_title, cloudAuthData.credentials))
    {
        return {};
    }

    return cloudAuthData;
}

QString FreshSessionTokenHelper::password() const
{
    return m_password;
}

} // namespace nx::vms::client::desktop
