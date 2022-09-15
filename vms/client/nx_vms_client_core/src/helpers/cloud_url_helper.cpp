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

using namespace nx::vms::utils;

QnCloudUrlHelper::QnCloudUrlHelper(
    SystemUri::ReferralSource source,
    SystemUri::ReferralContext context,
    QObject* parent)
    :
    QObject(parent),
    m_source(source),
    m_context(context)
{
}

QUrl QnCloudUrlHelper::mainUrl() const
{
    return makeUrl();
}

QUrl QnCloudUrlHelper::aboutUrl() const
{
    return makeUrl("/content/about");
}

QUrl QnCloudUrlHelper::accountManagementUrl() const
{
    return makeUrl("/account");
}

QUrl QnCloudUrlHelper::accountSecurityUrl() const
{
    return makeUrl("/account/security");
}

QUrl QnCloudUrlHelper::createAccountUrl() const
{
    return makeUrl("/register");
}

QUrl QnCloudUrlHelper::restorePasswordUrl() const
{
    return makeUrl("/restore_password");
}

QUrl QnCloudUrlHelper::faqUrl() const
{
    return makeUrl("/content/faq");
}

QUrl QnCloudUrlHelper::viewSystemUrl() const
{
    const auto systemId = qnClientCoreModule->commonModule()->globalSettings()->cloudSystemId();
    if (systemId.isEmpty())
        return mainUrl();

    return makeUrl(QString("/systems/%1/view").arg(systemId));
}

QUrl QnCloudUrlHelper::cloudLinkUrl(bool withReferral) const
{
    const QString url = nx::branding::customOpenSourceLibrariesUrl();
    if (!url.isEmpty())
        return url;

    return makeUrl("/content/libraries", withReferral);
}

QUrl QnCloudUrlHelper::makeUrl(const QString& path, bool withReferral) const
{
    SystemUri uri(nx::network::AppInfo::defaultCloudPortalUrl(
        nx::network::SocketGlobals::cloud().cloudHost()));

    if (withReferral)
        uri.setReferral(m_source, m_context);

    QUrl result(uri.toString());
    result.setPath(path);
    NX_DEBUG(this, result.toString());
    return result;
}
