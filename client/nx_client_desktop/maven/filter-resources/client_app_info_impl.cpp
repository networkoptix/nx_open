//
// This file is generated. Go to pom.xml.
//
#include <client/client_app_info.h>

#include "app_icons.h"

QString QnClientAppInfo::applicationName()
{
    return QStringLiteral("${product.title}");
}

QString QnClientAppInfo::applicationDisplayName()
{
    return QStringLiteral("${product.display.title}");
}

QString QnClientAppInfo::minilauncherBinaryName()
{
    return QStringLiteral("${minilauncher.binary.name}");
}

QString QnClientAppInfo::applauncherBinaryName()
{
    return QStringLiteral("${applauncher.binary.name}");
}

QString QnClientAppInfo::clientBinaryName()
{
    return QStringLiteral("${client.binary.name}");
}

QString QnClientAppInfo::installationRoot()
{
    return QStringLiteral("${installation.root}");
}

int QnClientAppInfo::videoWallIconId()
{
    return IDI_ICON_VIDEOWALL;
}

QString QnClientAppInfo::protocolHandlerBundleName()
{
    return QStringLiteral("${protocol_handler_app_name}");
}

QString QnClientAppInfo::protocolHandlerBundleIdBase()
{
    return QStringLiteral("${mac.protocol_handler_bundle.identifier}");
}

QString QnClientAppInfo::launcherVersionFile()
{
    return QStringLiteral("${launcher.version.file}");
}

QString QnClientAppInfo::binDirSuffix()
{
    #if defined(Q_OS_LINUX)
        return QStringLiteral("bin");
    #elif defined(Q_OS_MACX)
        return QStringLiteral("Contents/MacOS");
    #else
        return QString();
    #endif
}

QString QnClientAppInfo::libDirSuffix()
{
    #if defined(Q_OS_LINUX)
        return QStringLiteral("lib");
    #elif defined(Q_OS_MACX)
        return QStringLiteral("Contents/Frameworks");
    #else
        return QString();
    #endif
}
