#include <applauncher_app_info.h>

QString QnApplauncherAppInfo::applicationName()
{
    return QStringLiteral("${applauncher.name}");
}

QString QnApplauncherAppInfo::clientBinaryName()
{
    return QStringLiteral("${client.binary.name}");
}

QString QnApplauncherAppInfo::installationRoot()
{
    return QStringLiteral("${installation.root}");
}

QString QnApplauncherAppInfo::productName()
{
    return QStringLiteral("${display.product.name}");
}

QString QnApplauncherAppInfo::bundleName()
{
    return productName() + QStringLiteral(".app");
}
