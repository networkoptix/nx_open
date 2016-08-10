#include "cloud_url_helper.h"

#include <api/global_settings.h>

QUrl QnCloudUrlHelper::mainUrl()
{
    return QUrl(qnGlobalSettings->cloudPortalUrl());
}

QUrl QnCloudUrlHelper::aboutUrl()
{
    return QUrl(qnGlobalSettings->cloudPortalUrl() + lit("/content/about"));
}

QUrl QnCloudUrlHelper::accountManagementUrl()
{
    return QUrl(qnGlobalSettings->cloudPortalUrl() + lit("/account"));
}

QUrl QnCloudUrlHelper::createAccountUrl()
{
    return QUrl(qnGlobalSettings->cloudPortalUrl() + lit("/register"));
}

QUrl QnCloudUrlHelper::restorePasswordUrl()
{
    return QUrl(qnGlobalSettings->cloudPortalUrl() + lit("/restore_password"));
}
