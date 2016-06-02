#include "cloud_url_helper.h"

#include "api/global_settings.h"

QUrl QnCloudUrlHelper::mainUrl()
{
    return QUrl(qnGlobalSettings->cloudPortalUrl());
}

QUrl QnCloudUrlHelper::aboutUrl()
{
    return QUrl(qnGlobalSettings->cloudPortalUrl() + lit("/static/index.html#/about/cloud"));
}

QUrl QnCloudUrlHelper::accountManagementUrl()
{
    return QUrl(qnGlobalSettings->cloudPortalUrl() + lit("/static/index.html#/account"));
}

QUrl QnCloudUrlHelper::createAccountUrl()
{
    return QUrl(qnGlobalSettings->cloudPortalUrl() + lit("/static/index.html#/register"));
}

QUrl QnCloudUrlHelper::restorePasswordUrl()
{
    return QUrl(qnGlobalSettings->cloudPortalUrl() + lit("/static/index.html#/restore_password"));
}
