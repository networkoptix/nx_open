// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <nx/network/http/auth_tools.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_data.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

namespace nx::vms::client::desktop {

/**
* Displays password confirmation dialog for local user, or cloud OAuth dialog for cloud.
*
* Intended to use with server owner API calls
*/
class FreshSessionTokenHelper:
    public common::AbstractSessionTokenHelper
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
    FreshSessionTokenHelper(QWidget* parent, const QString& title, ActionType action);
    virtual ~FreshSessionTokenHelper() override;

    static common::SessionTokenHelperPtr makeHelper(
        QWidget* parent,
        const QString& title,
        const QString& mainText,
        const QString& actionText, //< Action button text.
        ActionType actionType);

    void setFlags(SessionRefreshFlags flags);

    std::optional<nx::network::http::AuthToken> refreshSession() override;

    core::CloudAuthData requestAuthData(std::function<bool()> closeCondition = {});

    virtual QString password() const override;

private:
    QPointer<QWidget> m_parent = nullptr;
    QString m_title;
    QString m_mainText;
    QString m_actionText;
    ActionType m_actionType = ActionType::updateSettings;
    QString m_password;
    SessionRefreshFlags m_flags;
};

} // namespace nx::vms::client::desktop
