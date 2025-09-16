// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth_login_dialog.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtCore/QUrlQuery>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStackedWidget>

#include <client/client_runtime_settings.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/oauth_client.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <utils/common/delayed.h>

#include "private/oauth_login_dialog_p.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr QSize kLoginDialogSize(1024, 768);
bool kCloudLoginDialogIsDisplayed = false;
constexpr int kCloseCheckIntervalMs = 250;

Qt::WindowFlags makeWindowFlags(SessionRefreshFlags flags)
{
    Qt::WindowFlags result = {};
    if (flags.testFlag(SessionRefreshFlag::stayOnTop))
        result |= Qt::WindowStaysOnTopHint;

    return result;
}

} // namespace

OauthLoginDialog::OauthLoginDialog(
    QWidget* parent,
    core::OauthClientType clientType,
    const QString& cloudSystem,
    SessionRefreshFlags flags)
:
    base_type(parent, makeWindowFlags(flags)),
    d(new OauthLoginDialogPrivate(this, clientType, cloudSystem))
{
    QSize size = kLoginDialogSize;
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(QMargins());

    if (d->m_urlLineEdit)
    {
        size.rheight() += d->m_urlLineEdit->sizeHint().height();
        layout->addWidget(d->m_urlLineEdit);
    }

    layout->addWidget(d->m_stackedWidget);
    resize(size);
    setHelpTopic(this, HelpTopic::Id::Login);

    executeLater([this]() { loadPage(); }, this);

    auto helper = d->oauthClient();
    connect(helper, &core::OauthClient::authDataReady, this, &OauthLoginDialog::authDataReady);
    connect(helper, &core::OauthClient::bindToCloudDataReady, this, &OauthLoginDialog::bindToCloudDataReady);
    connect(helper, &core::OauthClient::cloudTokensReady, this, &OauthLoginDialog::cloudTokensReady);
    connect(helper, &core::OauthClient::cancelled, this, &OauthLoginDialog::reject);
}

OauthLoginDialog::~OauthLoginDialog()
{
}

nx::vms::client::core::CloudAuthData OauthLoginDialog::login(
    QWidget* parent, const QString& title, const LoginParams& params)
{
    if (kCloudLoginDialogIsDisplayed)
        return {};

    const bool sessionAware = params.flags.testFlag(SessionRefreshFlag::sessionAware);

    QScopedValueRollback<bool> guard(kCloudLoginDialogIsDisplayed, true);

    std::unique_ptr<OauthLoginDialog> dialog = !sessionAware
        ? std::make_unique<OauthLoginDialog>(
            parent, params.clientType, params.cloudSystem, params.flags)
        : std::make_unique<QnSessionAware<OauthLoginDialog>>(
            parent, params.clientType, params.cloudSystem, params.flags);

    dialog->setWindowTitle(title);
    dialog->setCredentials(params.credentials);
    connect(
        dialog.get(),
        &OauthLoginDialog::authDataReady,
        dialog.get(),
        &OauthLoginDialog::accept);

    if (params.closeCondition)
    {
        QTimer* timer = new QTimer(dialog.get());
        connect(timer,
            &QTimer::timeout,
            dialog.get(),
            [closeCondition = params.closeCondition, dialog = dialog.get()]()
            {
                if (closeCondition())
                    dialog->reject();
            });
        timer->start(kCloseCheckIntervalMs);
    }

    if (!ini().modalOAuthDialog)
    {
        nx::vms::client::core::CloudAuthData authData;
        dialog->show();
        QEventLoop loop;

        connect(dialog.get(), &QDialog::finished, parent,
            [&](int result)
            {
                if (result == QDialog::Accepted)
                    authData = dialog->authData();
                loop.quit();
            });
        loop.exec();
        return authData;
    }

    if (dialog->exec() == QDialog::Accepted)
        return dialog->authData();

    return {};
}

bool OauthLoginDialog::validateToken(
    QWidget* parent,
    const QString& title,
    const nx::network::http::Credentials& credentials)
{
    std::unique_ptr<OauthLoginDialog> dialog =
        std::make_unique<OauthLoginDialog>(parent, core::OauthClientType::system2faAuth);

    dialog->setWindowTitle(title);
    dialog->setCredentials(credentials);
    connect(
        dialog.get(),
        &OauthLoginDialog::authDataReady,
        dialog.get(),
        &OauthLoginDialog::accept);

    if (!ini().modalOAuthDialog)
    {
        bool success = false;
        dialog->show();
        QEventLoop loop;

        connect(dialog.get(), &QDialog::finished, parent,
            [&](int result)
            {
                success = (result == QDialog::Accepted);
                loop.quit();
            });
        loop.exec();
        return success;
    }

    return (dialog->exec() == QDialog::Accepted);
}

void OauthLoginDialog::loadPage()
{
    d->load(d->oauthClient()->url());
}

const nx::vms::client::core::CloudAuthData& OauthLoginDialog::authData() const
{
    return d->oauthClient()->authData();
}

const nx::vms::client::core::CloudBindData& OauthLoginDialog::cloudBindData() const
{
    return d->oauthClient()->cloudBindData();
}

const nx::vms::client::core::CloudTokens& OauthLoginDialog::cloudTokens() const
{
    return d->oauthClient()->cloudTokens();
}

void OauthLoginDialog::setCredentials(const nx::network::http::Credentials& credentials)
{
    d->oauthClient()->setCredentials(credentials);
}

void OauthLoginDialog::setSystemName(const QString& systenName)
{
    d->oauthClient()->setSystemName(systenName);
}

} // namespace nx::vms::client::desktop
