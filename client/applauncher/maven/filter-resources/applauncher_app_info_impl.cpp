#include <applauncher_app_info.h>

QString QnApplauncherAppInfo::applicationName()
{
    return QStringLiteral("${product.title}");
}

QString QnApplauncherAppInfo::clientBinaryName()
{
    return QStringLiteral("${client.binary.name}");
}

QString QnApplauncherAppInfo::mirrorListUrl()
{
    return QStringLiteral("${mirrorListUrl}");
}