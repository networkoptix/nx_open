#include "protocol_handler.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <CoreServices/CoreServices.h>

#include <platform/core_foundation_mac/cf_url.h>
#include <platform/core_foundation_mac/cf_string.h>
#include <platform/core_foundation_mac/cf_array.h>
#include <nx/utils/software_version.h>

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

bool isBundleExist(const QString& bundleId)
{
    const auto cfBundleId = cf::QnCFString(bundleId);
    const cf::QnCFArray urls(LSCopyApplicationURLsForBundleIdentifier(cfBundleId.ref(), nullptr));
    int realCount = urls.size();
    const auto count = urls.size();
    for (int i = 0; i != count; ++i)
    {
        const auto path = cf::QnCFUrl::toString(urls.at<CFURLRef>(i));
        if (path.startsWith(lit("file:///Volumes"))) //< Excludes systems from mounted 'dmg' disks
            --realCount;
    }
    return (realCount > 0);
}

QStringList getBundlesForProtocol(const QString& protocol)
{
    const auto cfProtocol = cf::QnCFString(protocol);
    const cf::QnCFArray bundleIds(LSCopyAllHandlersForURLScheme(cfProtocol.ref()));

    QStringList result;
    const int count = bundleIds.size();
    for (int i = 0; i != count; ++i)
    {
        const auto pathRef = bundleIds.at<CFStringRef>(i);
        const auto cfBundleId = cf::QnCFString::makeOwned(pathRef);
        result.push_back(cfBundleId.toString());
    }
    return result;
}

} // Unnamed namespace

bool nx::vms::utils::registerSystemUriProtocolHandler(
    const QString& protocol,
    const QString& applicationBinaryPath,
    const QString& applicationName,
    const QString& macHandlerBundleIdBase,
    const QString& description,
    const QString& customization,
    const nx::utils::SoftwareVersion& version)
{
    Q_UNUSED(applicationName);
    Q_UNUSED(customization);
    Q_UNUSED(description);

    int currentBuild = version.build();

    // Looking for maximal available version of application and reregister it anyway
    const auto currentHandlers = getBundlesForProtocol(protocol);
    for(const auto& bundleId: currentHandlers)
    {
        /** Checks if it is NX handlers only.
          * We HAVE TO use insensetive case for string compare because from time to time
          * LSCopyApplicationURLsForBundleIdentifier returns bundle ids in lower case only
          */
        if (!bundleId.startsWith(macHandlerBundleIdBase, Qt::CaseInsensitive))
            continue;

        if (!isBundleExist(bundleId)) //< Skips removed apps
            continue;

        const int buildNumberLen = (bundleId.size() - macHandlerBundleIdBase.size());
        if (buildNumberLen <= 0)
            continue;

        // We can rely on build number - it will always growing
        bool correctConversion = false;
        const int buildNumber = bundleId.right(buildNumberLen).toInt(&correctConversion);
        if (!correctConversion)
            continue;

        if (buildNumber > currentBuild)
            currentBuild = buildNumber;
    }

    const auto handlerBundleId = lit("%1%2").arg(
        macHandlerBundleIdBase, QString::number(currentBuild));

    bool result = true;
    if (currentBuild == version.build())
        result = registerAsLaunchService(applicationBinaryPath);

    const auto cfId = cf::QnCFString(handlerBundleId);
    return  (result
        && (LSSetDefaultHandlerForURLScheme(cf::QnCFString(protocol).ref(), cfId.ref()) != noErr));
}

