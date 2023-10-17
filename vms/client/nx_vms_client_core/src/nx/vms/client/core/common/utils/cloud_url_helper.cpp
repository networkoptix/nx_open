// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_url_helper.h"

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <nx/branding.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/utils/system_uri.h>

namespace nx::vms::client::core {

using nx::vms::utils::SystemUri;

CloudUrlHelper::CloudUrlHelper(
    SystemUri::ReferralSource source,
    SystemUri::ReferralContext context,
    QObject* parent)
    :
    QObject(parent),
    m_source(source),
    m_context(context)
{
}

QUrl CloudUrlHelper::mainUrl() const
{
    return makeUrl();
}

QUrl CloudUrlHelper::aboutUrl() const
{
    return makeUrl("/content/about");
}

QUrl CloudUrlHelper::accountManagementUrl() const
{
    return makeUrl("/account");
}

QUrl CloudUrlHelper::accountSecurityUrl() const
{
    return makeUrl("/account/security");
}

QUrl CloudUrlHelper::createAccountUrl() const
{
    return makeUrl("/register");
}

QUrl CloudUrlHelper::restorePasswordUrl() const
{
    return makeUrl("/restore_password");
}

QUrl CloudUrlHelper::faqUrl() const
{
    return makeUrl("/content/faq");
}

QUrl CloudUrlHelper::viewSystemUrl() const
{
    const auto systemId = qnClientCoreModule->globalSettings()->cloudSystemId();
    if (systemId.isEmpty())
        return mainUrl();

    return makeUrl(QString("/systems/%1/view").arg(systemId));
}

QUrl CloudUrlHelper::cloudLinkUrl(bool withReferral) const
{
    const QString url = nx::branding::customOpenSourceLibrariesUrl();
    if (!url.isEmpty())
        return url;

    return makeUrl("/content/libraries", withReferral);
}

QUrl CloudUrlHelper::makeUrl(const QString& path, bool withReferral) const
{
    SystemUri uri(nx::network::AppInfo::defaultCloudPortalUrl(
        nx::network::SocketGlobals::cloud().cloudHost()));

    if (withReferral)
        uri.referral = {m_source, m_context};

    QUrl result(uri.toString());
    result.setPath(path);
    NX_DEBUG(this, result.toString());
    return result;
}

} // namespace nx::vms::client::core
