//
// This file is generated. Go to pom.xml.
//
#include <media_server/media_server_app_info.h>

QString QnServerAppInfo::applicationName()
{
    return QStringLiteral("${product.title}");
}

QString QnServerAppInfo::applicationDisplayName()
{
    return QStringLiteral("${product.display.title}");
}

QString QnServerAppInfo::serviceName()
{
    return QStringLiteral("${mediaserver_service_name}");
}
