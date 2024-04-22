// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <nx/network/http/auth_tools.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

namespace nx::vms::client::desktop {

/**
* Displays password confirmation dialog for local user, or cloud OAuth dialog for cloud.
*
* Intended to use with server owner API calls
*/
class FreshSessionTokenHelper:
    public common::AbstractSessionTokenHelper,
    public nx::vms::client::core::RemoteConnectionAware
{
public:
    enum class ActionType
    {
        merge,
        unbind,
        backup,
        restore,
        updateSettings,
        issueRefreshToken,
    };

public:
    FreshSessionTokenHelper(QWidget* parent);
    virtual ~FreshSessionTokenHelper() override;

    static common::SessionTokenHelperPtr makeHelper(
        QWidget* parent,
        const QString& title,
        const QString& mainText,
        const QString& actionText, //< Action button text.
        ActionType actionType);

    static FreshSessionTokenHelper* makeFreshSessionTokenHelper(
        QWidget* parent,
        const QString& title,
        const QString& mainText,
        const QString& actionText,
        ActionType actionType);

    std::optional<nx::network::http::AuthToken> refreshToken() override;
    core::CloudAuthData requestAuthData();

    virtual QString password() const override;

private:

    QPointer<QWidget> m_parent = nullptr;
    QString m_title;
    QString m_mainText;
    QString m_actionText;
    ActionType m_actionType = ActionType::updateSettings;
    QString m_password;
};

} // namespace nx::vms::client::desktop
