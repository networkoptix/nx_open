#include "cloud_url_helper.h"

#include <api/global_settings.h>

#include <watchers/cloud_status_watcher.h>

#include <nx/vms/utils/system_uri.h>

namespace {

QUrl makeUrl(const QString& path = QString(), bool auth = true)
{
    using namespace nx::vms::utils;

    SystemUri uri(qnGlobalSettings->cloudPortalUrl());
    if (auth)
        uri.setAuthenticator(qnCloudStatusWatcher->temporaryLogin(), qnCloudStatusWatcher->temporaryPassword());
    uri.setReferral(SystemUri::ReferralSource::DesktopClient, SystemUri::ReferralContext::CloudMenu);
    QUrl result = uri.toUrl();
    result.setPath(path);
    return result;
}

}

QUrl QnCloudUrlHelper::mainUrl()
{
    return makeUrl();
}

QUrl QnCloudUrlHelper::aboutUrl()
{
    return makeUrl(lit("/content/about"), false);
}

QUrl QnCloudUrlHelper::accountManagementUrl()
{
    return makeUrl(lit("/account"));
}

QUrl QnCloudUrlHelper::createAccountUrl()
{
    return makeUrl(lit("/register"), false);
}

QUrl QnCloudUrlHelper::restorePasswordUrl()
{
    return makeUrl(lit("/restore_password"));
}
