// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth.h"

#include <QtWidgets/QPushButton>

#include <client/client_settings.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/views/resource_list_view.h>

namespace nx::vms::client::desktop::jsapi {

Auth::Auth(UrlProvider urlProvider, const QnResourcePtr& resource, QObject* parent):
    base_type(parent),
    m_url(urlProvider),
    m_resource(resource)
{
}

bool Auth::isApproved(const QUrl& url) const
{
    auto allowedUrls = qnSettings->authAllowedUrls();
    const auto key = AuthAllowedUrls::key(
        connection()->moduleInformation().localSystemId,
        QString::fromStdString(connectionCredentials().username));

    if (allowedUrls.items[key].contains(url.toString()))
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Icon::Warning,
        tr("This webpage is requesting access to your account for authorization"),
        tr("Your confirmation is required to provide a token to",
            "... a web page (below there is a web page name with an icon)"),
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        /*parent*/ nullptr);

    messageBox.addCustomWidget(new QnResourceListView({m_resource}));
    messageBox.setCheckBoxEnabled(true); //< "Do not show this message again" checkbox.
    const auto allowButton = messageBox.addButton(tr("Allow"), QDialogButtonBox::YesRole);

    messageBox.exec();
    const bool isApproved = messageBox.clickedButton() == allowButton;

    if (isApproved && messageBox.isChecked())
    {
        allowedUrls.items[key].insert(url.toString());
        qnSettings->setAuthAllowedUrls(allowedUrls);
    }

    return isApproved;
}

QString Auth::sessionToken() const
{
    if (!NX_ASSERT(m_url))
        return {};

    const auto token = connectionCredentials().authToken;
    if (token.isBearerToken() && isApproved(m_url()))
        return QString::fromStdString(token.value);

    return {};
}

} // namespace nx::vms::client::desktop::jsapi
