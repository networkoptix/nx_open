#include <applauncher_app_info.h>

QString QnApplauncherAppInfo::applicationName()
{
    return QStringLiteral("${applauncher.name}");
}

QString QnApplauncherAppInfo::clientLauncherName()
{
    return QStringLiteral("${client_launcher_name}");
}

QString QnApplauncherAppInfo::installationRoot()
{
    return QStringLiteral("${installation.root}");
}

QString QnApplauncherAppInfo::productName()
{
    return QStringLiteral("${customization.vmsName}");
}

QString QnApplauncherAppInfo::bundleName()
{
    return productName() + QStringLiteral(".app");
}
