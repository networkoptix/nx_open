//
// This file is generated. Go to pom.xml.
//
#include <media_server/media_server_app_info.h>

QString QnServerAppInfo::applicationName()
{
    return QStringLiteral("${mediaserver_application_name}");
}

QString QnServerAppInfo::applicationDisplayName()
{
    return QStringLiteral("${mediaserver.display.name}");
}

QString QnServerAppInfo::serviceName()
{
    return QStringLiteral("${mediaserver_service_name}");
}
