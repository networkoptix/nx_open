
#include "protocol_handler.h"

#if defined(Q_OS_MAC)

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QSet>

#include <CoreServices/CoreServices.h>

#include <platform/core_foundation/cf_url.h>
#include <platform/core_foundation/cf_string.h>

#include <utils/common/app_info.h>
#include <utils/common/software_version.h>

namespace {

// Searches for bundle path. Bundle path should be a folder with ".app" ending
QString getBundlePath()
{
    const auto isBundleDir = [](const QDir& dir)
    {
        static const auto kBundleSuffix = lit(".app");
        return (dir.dirName().right(kBundleSuffix.size()) == kBundleSuffix);
    };

    QDir dir(QCoreApplication::applicationDirPath());
    while(!isBundleDir(dir) && !dir.dirName().isEmpty())
        dir.cdUp();

    return dir.absolutePath();
}

bool registerAsLaunchService()
{
    const auto bundlePath = getBundlePath();
    if (bundlePath.isEmpty())
        return false;

    const auto bundleUrl = cf::QnCFUrl::createFileUrl(bundlePath);
    const auto status = LSRegisterURL(bundleUrl.ref(), true);
    return (status == noErr);
}

} // Unnamed namespace

bool nx::utils::registerSystemUriProtocolHandler(
    const QString& protocol
    , const QString& bundleId)
{
    return registerAsLaunchService() &&
        LSSetDefaultHandlerForURLScheme(cf::QnCFString(protocol).ref(),
            cf::QnCFString(bundleId).ref());
}

#endif
