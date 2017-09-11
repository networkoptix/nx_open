#pragma once

#include <QtCore/QCoreApplication>

#include <helpers/cloud_url_helper.h>
#include <nx/network/app_info.h>
#include <utils/common/html.h>

class QnCloudResultMessages
{
    Q_DECLARE_TR_FUNCTIONS(QnCloudResultMessages)

public:
    static QString invalidCredentials()
    {
        return tr("Incorrect email or password");
    }

    static QString accountNotActivated()
    {
        using nx::vms::utils::SystemUri;
        QnCloudUrlHelper urlHelper(
            SystemUri::ReferralSource::DesktopClient,
            SystemUri::ReferralContext::SettingsDialog);

        const auto portalText = tr("%1 Portal", "%1 is the cloud name (like 'Nx Cloud')")
            .arg(nx::network::AppInfo::cloudName()).replace(QChar(L' '), lit("&nbsp;"));

        const auto portalLink = makeHref(portalText, urlHelper.mainUrl());
        return tr("Account isn't activated. Please log in to %1 and follow provided instructions",
            "%1 is the cloud portal name (like 'Nx Cloud Portal'").arg(portalLink);
   }
};
