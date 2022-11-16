// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_api_auth_dialog.h"

#include <QtWidgets/QPushButton>

#include <client/client_settings.h>
#include <core/resource/resource.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>
#include <ui/widgets/views/resource_list_view.h>

namespace nx::vms::client::desktop {

ClientApiAuthDialog::ClientApiAuthDialog(const QnResourcePtr& resource, QWidget* parent):
    QnMessageBox(
        QnMessageBox::Icon::Warning,
        tr("This web page is requesting access to your account for authorization"),
        tr("Your confirmation is required to provide a token to",
            "... a web page (below there is a web page name with an icon)"),
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        parent)

{
    addCustomWidget(new QnResourceListView({resource}));
    setCheckBoxEnabled(true); //< "Do not show this message again" checkbox.
    m_allowButton = addButton(tr("Allow"), QDialogButtonBox::YesRole);
}

bool ClientApiAuthDialog::isApproved(const QnResourcePtr& resource, const QUrl& url)
{
    const auto systemContext = dynamic_cast<core::SystemContext*>(resource->systemContext());
    if (!NX_ASSERT(systemContext))
        return false;

    const auto connection = systemContext->connection();
    if (!connection)
        return false;

    const auto key = AuthAllowedUrls::key(
        systemContext->moduleInformation().localSystemId,
        QString::fromStdString(connection->credentials().username));

    auto allowedUrls = qnSettings->authAllowedUrls();
    if (allowedUrls.items[key].contains(url.toString()))
        return true;

    ClientApiAuthDialog dialog(resource);
    dialog.exec();
    const bool isApproved = dialog.clickedButton() == dialog.m_allowButton;

    if (isApproved && dialog.isChecked())
    {
        allowedUrls.items[key].insert(url.toString());
        qnSettings->setAuthAllowedUrls(allowedUrls);
    }

    return isApproved;
}

} // nx::vms::client::desktop
