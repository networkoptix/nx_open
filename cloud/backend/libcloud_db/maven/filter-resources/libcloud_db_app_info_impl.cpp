#include "nx/cloud/db/libcloud_db_app_info.h"

QString QnLibCloudDbAppInfo::applicationName()
{
    return QStringLiteral("${customization.companyName} libcloud_db");
}

QString QnLibCloudDbAppInfo::applicationDisplayName()
{
    return QStringLiteral("${customization.companyName} Cloud Database");
}
