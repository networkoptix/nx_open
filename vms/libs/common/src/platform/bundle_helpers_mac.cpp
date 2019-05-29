#include "bundle_helpers_mac.h"

#if defined(Q_OS_MACX)

#include <ApplicationServices/ApplicationServices.h>

#include <QtCore/QVector>

#include <nx/utils/platform/core_foundation_mac/cf_url.h>
#include <nx/utils/platform/core_foundation_mac/cf_string.h>

namespace {

const auto kAppClassTag = cf::QnCFString("NSPrincipalClass");
const auto kHiDpiSupportTag = cf::QnCFString("NSHighResolutionCapable");

}

bool QnBundleHelpers::isHiDpiSupported()
{
    uint32_t displaysCount = 0;
    if (CGGetOnlineDisplayList(0, nullptr, &displaysCount) != kCGErrorSuccess)
        return true;

    typedef QVector<CGDirectDisplayID> DisplayIDs;
    DisplayIDs displayIDs(displaysCount, CGDirectDisplayID());
    if (CGGetOnlineDisplayList(displaysCount, displayIDs.data(), nullptr) != kCGErrorSuccess)
        return true;

    typedef QVector<int> WidthsVector;

    WidthsVector displayWidths;
    for (const auto id: displayIDs)
    {
        typedef cf::QnCFRefHolder<CFArrayRef> ArrayType;
        ArrayType modes(CGDisplayCopyAllDisplayModes(id, nullptr));
        if (!modes.ref())
            continue;

        // TODO: #ynikitenkov add QnCFArray class
        const auto modesCount = CFArrayGetCount(modes.ref());
        if (modesCount < 0)
            continue;

        for (auto i = 0; i != modesCount; ++i)
        {
            auto modeHandle = const_cast<CGDisplayModeRef>(
                static_cast<const CGDisplayMode*>(CFArrayGetValueAtIndex(modes.ref(), i)));
            if (!modeHandle)
                continue;

            static const auto kFullHDWidth = 1920;
            if (CGDisplayModeGetWidth(modeHandle) > kFullHDWidth)
                return true;
        }
    }

    return false;
}

/**
 * It is supposed that Info.plist contains NSPrincipalClass field with value NSApplication
 * value and NSHighResolutionCapable field set to "True" or "False"
**/
bool QnBundleHelpers::isInHiDpiMode(const QString& path)
{
    const auto url = cf::QnCFUrl::createFileUrl(path);

    // TODO: #ynikitenkov Add QnCFBundle class
    const cf::QnCFRefHolder<CFBundleRef> bundle(CFBundleCreate(kCFAllocatorDefault, url.ref()));
    if (!bundle.ref())
        return true;    // By default, if there is no Info.plist, it is run in HiDpi mode

    // TODO: #ynikitenkov Add QnCFBoolean class
    const cf::QnCFRefHolder<CFBooleanRef> isHiDpiRef(static_cast<CFBooleanRef>(
        CFBundleGetValueForInfoDictionaryKey(bundle.ref(), kHiDpiSupportTag.ref())));

    if (!isHiDpiRef.ref())
    {
        const cf::QnCFString appClassRef(static_cast<CFStringRef>(
            CFBundleGetValueForInfoDictionaryKey(bundle.ref(), kAppClassTag.ref())));

        if (appClassRef.ref())
            return true;    // HiDpi mode if any class specified without mode
        else
            return false;   //< If both application class and mode fields
                            // don't exist - there is no HiDpi mode
    }

    return static_cast<bool>(CFBooleanGetValue(isHiDpiRef.ref()));
}

#endif // defined(Q_OS_MACX)
