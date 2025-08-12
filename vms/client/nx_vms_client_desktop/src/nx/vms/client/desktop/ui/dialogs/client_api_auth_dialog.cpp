// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_api_auth_dialog.h"

#include <QtWidgets/QPushButton>

#include <core/resource/user_resource.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/common/html/html.h>
#include <ui/widgets/views/resource_list_view.h>

namespace nx::vms::client::desktop {

namespace {

QString currentUserKey()
{
    if (auto context = appContext()->currentSystemContext())
    {
        if (auto user = context->user())
        {
            if (user->isCloud())
                return user->getName();

            if (user->isTemporary())
                return QString(); // Do not save temporary users allowance.

            return context->moduleInformation().localSystemId.toString(QUuid::WithBraces)
                + user->getName();
        }
    }
    return QString();
}

} // namespace

ClientApiAuthDialog::ClientApiAuthDialog(const QString& origin, QWidget* parent):
    QnMessageBox(
        QnMessageBox::Icon::Warning,
        tr("This web page is requesting access to your account for authorization"),
        tr("Your confirmation is required to provide a token to %1",
            "%1 is a url hostname").arg(common::html::bold(origin)),
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        parent)
{
    m_allowButton = addButton(tr("Allow"), QDialogButtonBox::YesRole);
}

bool ClientApiAuthDialog::isApproved(const QUrl& url)
{
    const auto key = currentUserKey();
    const bool canSaveAllowance = !key.isEmpty();

    const AuthAllowedUrls::Origin origin = AuthAllowedUrls::origin(url);

    auto allowedUrls = appContext()->localSettings()->authAllowedUrls();
    if (canSaveAllowance && allowedUrls.items[key].contains(origin))
        return true;

    ClientApiAuthDialog dialog(origin);
    dialog.setCheckBoxEnabled(canSaveAllowance); //< "Do not show this message again" checkbox.
    dialog.exec();

    const bool isApproved = dialog.clickedButton() == dialog.m_allowButton;

    if (isApproved && canSaveAllowance && dialog.isChecked())
    {
        allowedUrls.items[key].insert(origin);
        appContext()->localSettings()->authAllowedUrls = allowedUrls;
    }

    return isApproved;
}

} // nx::vms::client::desktop
