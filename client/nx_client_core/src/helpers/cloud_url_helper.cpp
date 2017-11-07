#include "cloud_url_helper.h"

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <nx/network/app_info.h>
#include <nx/vms/utils/system_uri.h>

#include <api/global_settings.h>
#include <utils/common/app_info.h>
#include <watchers/cloud_status_watcher.h>

#include <nx/utils/log/log.h>

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

nx::utils::Url QnCloudUrlHelper::mainUrl() const
{
    return makeUrl();
}

nx::utils::Url QnCloudUrlHelper::aboutUrl() const
{
    return makeUrl(lit("/content/about"), false);
}

nx::utils::Url QnCloudUrlHelper::accountManagementUrl() const
{
    return makeUrl(lit("/account"));
}

nx::utils::Url QnCloudUrlHelper::createAccountUrl() const
{
    return makeUrl(lit("/register"), false);
}

nx::utils::Url QnCloudUrlHelper::restorePasswordUrl() const
{
    return makeUrl(lit("/restore_password"), false);
}

nx::utils::Url QnCloudUrlHelper::faqUrl() const
{
    return makeUrl(lit("/content/faq"), false);
}

nx::utils::Url QnCloudUrlHelper::viewSystemUrl() const
{
    const auto systemId = qnClientCoreModule->commonModule()->globalSettings()->cloudSystemId();
    if (systemId.isEmpty())
        return mainUrl();

    return makeUrl(lit("/systems/%1/view").arg(systemId));
}

nx::utils::Url QnCloudUrlHelper::makeUrl(const QString& path, bool auth) const
{
    SystemUri uri(nx::network::AppInfo::defaultCloudPortalUrl());

    if (auth)
    {
        auto credentials = qnCloudStatusWatcher->createTemporaryCredentials();
        uri.setAuthenticator(credentials.user, credentials.password.value());
    }

    uri.setReferral(m_source, m_context);

    nx::utils::Url result = uri.toUrl();
    result.setPath(path);
    NX_DEBUG(typeid(QnCloudUrlHelper), result.toString());
    return result;
}
