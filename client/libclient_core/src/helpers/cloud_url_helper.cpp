#include "cloud_url_helper.h"

#include <nx/vms/utils/system_uri.h>
#include <api/global_settings.h>
#include <utils/common/app_info.h>
#include <watchers/cloud_status_watcher.h>

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
    return makeUrl(lit("/content/about"), false);
}

QUrl QnCloudUrlHelper::accountManagementUrl() const
{
    return makeUrl(lit("/account"));
}

QUrl QnCloudUrlHelper::createAccountUrl() const
{
    return makeUrl(lit("/register"), false);
}

QUrl QnCloudUrlHelper::restorePasswordUrl() const
{
    return makeUrl(lit("/restore_password"));
}

QUrl QnCloudUrlHelper::makeUrl(const QString& path, bool auth) const
{
    SystemUri uri(QnAppInfo::defaultCloudPortalUrl());

    if (auth)
    {
        auto credentials = qnCloudStatusWatcher->createTemporaryCredentials();
        uri.setAuthenticator(credentials.user, credentials.password);
    }

    uri.setReferral(m_source, m_context);

    QUrl result = uri.toUrl();
    result.setPath(path);
    return result;
}
