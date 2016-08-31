
#include "bundle_helpers_mac.h"

#include <platform/core_foundation_mac/cf_url.h>
#include <platform/core_foundation_mac/cf_string.h>

namespace {

const auto kAppClassTag = cf::QnCFString(lit("NSPrincipalClass"));
const auto kHiDpiSupportTag = cf::QnCFString(lit("NSHighResolutionCapable"));

const auto kAppCalssAplicationValue = lit("NSApplication");

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
