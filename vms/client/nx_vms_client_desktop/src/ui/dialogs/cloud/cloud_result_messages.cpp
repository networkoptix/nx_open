// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_result_messages.h"

#include <nx/branding.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/common/html/html.h>

QString QnCloudResultMessages::accountNotFound()
{
    return tr("Account not found");
}

QString QnCloudResultMessages::accountNotActivated()
{
    using nx::network::AppInfo;
    using namespace nx::vms::client::desktop;

    const auto cloudLink = nx::vms::common::html::customLink(
        nx::branding::cloudName(),
        QString::fromStdString(
            AppInfo::defaultCloudPortalUrl(nx::network::SocketGlobals::cloud().cloudHost())));

    return tr("Account is not activated.")
        + ' '
        + tr("Please log in to %1 and follow the provided instructions.",
            "%1 is a cloud site name like \"Nx Cloud\"").arg(cloudLink);
}

QString QnCloudResultMessages::invalidPassword()
{
    return tr("Invalid password");
}

QString QnCloudResultMessages::userLockedOut()
{
    return tr("Too many attempts. Try again in a minute.");
}
