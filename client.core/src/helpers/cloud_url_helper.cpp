#include "cloud_url_helper.h"

#include "api/global_settings.h"

QUrl QnCloudUrlHelper::mainUrl()
{
    return QUrl(QnGlobalSettings::instance()->cloudPortalUrl());
}

QUrl QnCloudUrlHelper::aboutUrl()
{
    return QUrl(QnGlobalSettings::instance()->cloudPortalUrl() + lit("/static/index.html#/about/cloud"));
}

QUrl QnCloudUrlHelper::accountManagementUrl()
{
    return QUrl(QnGlobalSettings::instance()->cloudPortalUrl() + lit("/static/index.html#/account"));
}

QUrl QnCloudUrlHelper::createAccountUrl()
{
    return QUrl(QnGlobalSettings::instance()->cloudPortalUrl() + lit("/static/index.html#/register"));
}

QUrl QnCloudUrlHelper::restorePasswordUrl()
{
    return QUrl(QnGlobalSettings::instance()->cloudPortalUrl() + lit("/static/index.html#/restore_password"));
}
