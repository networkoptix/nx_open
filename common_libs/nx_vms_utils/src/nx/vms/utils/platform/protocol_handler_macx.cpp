#include "protocol_handler.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <CoreServices/CoreServices.h>

#include <platform/core_foundation_mac/cf_url.h>
#include <platform/core_foundation_mac/cf_string.h>

#include <QDebug>

namespace {

// Searches for bundle path. Bundle path should be a folder with ".app" ending
QString getBundlePath(const QString& applicationPath)
{
    static const auto kBundleSuffix = lit(".app");

    auto dir = QDir(applicationPath);
    while(!dir.dirName().endsWith(kBundleSuffix) && !dir.dirName().isEmpty())
        dir.cdUp();

    return dir.absolutePath();
}

bool registerAsLaunchService(const QString& applicationPath)
{
    const auto bundlePath = getBundlePath(applicationPath);
    if (bundlePath.isEmpty())
        return false;

    const auto bundleUrl = cf::QnCFUrl::createFileUrl(bundlePath);
    const auto status = LSRegisterURL(bundleUrl.ref(), true);
    return (status == noErr);
}

} // Unnamed namespace

bool nx::vms::utils::registerSystemUriProtocolHandler(
    const QString& protocol,
    const QString& applicationBinaryPath,
    const QString& applicationName,
    const QString& macOsBundleId,
    const QString& description,
    const QString& customization,
    const nx::utils::SoftwareVersion& version)
{
    Q_UNUSED(applicationName);
    Q_UNUSED(customization);
    Q_UNUSED(description);
    Q_UNUSED(version);

    return registerAsLaunchService(applicationBinaryPath) &&
        LSSetDefaultHandlerForURLScheme(cf::QnCFString(protocol).ref(),
            cf::QnCFString(macOsBundleId).ref());
}

