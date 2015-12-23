#include "cloud_url_helper.h"

#include <utils/common/app_info.h>

QUrl QnCloudUrlHelper::mainUrl()
{
    return QUrl(QnAppInfo::cloudPortalUrl());
}

QUrl QnCloudUrlHelper::aboutUrl()
{
    return QUrl(QnAppInfo::cloudPortalUrl() + lit("/static/index.html#/about/cloud"));
}

QUrl QnCloudUrlHelper::accountManagementUrl()
{
    return QUrl(QnAppInfo::cloudPortalUrl() + lit("/static/index.html#/account"));
}

QUrl QnCloudUrlHelper::createAccountUrl()
{
    return QUrl(QnAppInfo::cloudPortalUrl() + lit("/static/index.html#/register"));
}

QUrl QnCloudUrlHelper::restorePasswordUrl()
{
    return QUrl(QnAppInfo::cloudPortalUrl() + lit("/static/index.html#/restore_password"));
}
