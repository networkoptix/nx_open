// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth_login_dialog.h"

#include <QtCore/QUrlQuery>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStackedWidget>

#include <client/client_runtime_settings.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/client/core/network/oauth_client.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <nx/utils/log/log.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <utils/common/delayed.h>

#include "private/oauth_login_dialog_p.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr QSize kLoginDialogSize(480, 480);

} // namespace

OauthLoginDialog::OauthLoginDialog(
    QWidget* parent,
    core::OauthClientType clientType,
    const QString& cloudSystem)
:
    base_type(parent, Qt::MSWindowsFixedSizeDialogHint),
    d(new OauthLoginDialogPrivate(this, clientType, cloudSystem))
{
    QSize fixedSize = kLoginDialogSize;
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(QMargins());

    if (d->m_urlLineEdit)
    {
        fixedSize.rheight() += d->m_urlLineEdit->sizeHint().height();
        layout->addWidget(d->m_urlLineEdit);
    }

    layout->addWidget(d->m_stackedWidget);
    setFixedSize(fixedSize);
    setHelpTopic(this, Qn::Login_Help);

    executeLater([this]() { loadPage(); }, this);

    const auto helper = d->oauthClient();
    connect(helper, &core::OauthClient::authDataReady, this, &OauthLoginDialog::authDataReady);
    connect(helper, &core::OauthClient::cancelled, this, &OauthLoginDialog::reject);
}

OauthLoginDialog::~OauthLoginDialog()
{
}

nx::vms::client::core::CloudAuthData OauthLoginDialog::login(
    QWidget* parent,
    const QString& title,
    core::OauthClientType clientType,
    bool sessionAware,
    const QString& cloudSystem)
{
    std::unique_ptr<OauthLoginDialog> dialog = !sessionAware
        ? std::make_unique<OauthLoginDialog>(parent, clientType, cloudSystem)
        : std::make_unique<QnSessionAware<OauthLoginDialog>>(parent, clientType, cloudSystem);
    dialog->setWindowTitle(title);
    connect(
        dialog.get(),
        &OauthLoginDialog::authDataReady,
        dialog.get(),
        &OauthLoginDialog::accept);

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

    return (dialog->exec() == QDialog::Accepted);
}

void OauthLoginDialog::loadPage()
{
    d->load(nx::utils::Url::fromQUrl(d->oauthClient()->url()));
}

const nx::vms::client::core::CloudAuthData& OauthLoginDialog::authData() const
{
    return d->oauthClient()->authData();
}

void OauthLoginDialog::setCredentials(const nx::network::http::Credentials& credentials)
{
    d->oauthClient()->setCredentials(credentials);
}

} // namespace nx::vms::client::desktop
