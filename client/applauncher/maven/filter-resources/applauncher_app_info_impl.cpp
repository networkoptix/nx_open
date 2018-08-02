#include <applauncher_app_info.h>

QString QnApplauncherAppInfo::applicationName()
{
    return QStringLiteral("${applauncher.name}");
}

QString QnApplauncherAppInfo::clientBinaryName()
{
    return QStringLiteral("${client.binary.name}");
}

QString QnApplauncherAppInfo::mirrorListUrl()
{
    return QStringLiteral("${mirrorListUrl}");
}

QString QnApplauncherAppInfo::installationRoot()
{
    return QStringLiteral("${installation.root}");
}

#if defined(Q_OS_MACX)
QString QnApplauncherAppInfo::productName()
{
    return QStringLiteral("${display.product.name}");
}

QString QnApplauncherAppInfo::bundleName()
{
    return productName() + QStringLiteral(".app");
}
#endif
