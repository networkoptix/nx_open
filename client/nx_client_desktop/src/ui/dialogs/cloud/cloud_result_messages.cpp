#include "cloud_result_messages.h"

#include <nx/network/app_info.h>

QString QnCloudResultMessages::accountNotFound()
{
    return tr("Account not found");
}

QString QnCloudResultMessages::accountNotActivated()
{
    using nx::network::AppInfo;

    const auto cloudLink = lit("<a href=\"%2\">%1</a>").arg(
        AppInfo::cloudName(),
        AppInfo::defaultCloudPortalUrl(AppInfo::defaultCloudHostName()));

    return tr("Account is not activated.")
        + L' '
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
