#include <applauncher_app_info.h>

QString QnApplauncherAppInfo::applicationName()
{
    return QStringLiteral("${product.display.name}");
}

QString QnApplauncherAppInfo::clientBinaryName()
{
    return QStringLiteral("${client.binary.name}");
}
