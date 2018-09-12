#pragma once

#include <QtCore/QString>

namespace nx {
namespace vms {
namespace utils {

struct NX_VMS_UTILS_API AppInfo
{
    /** Native protocol for URI links. */
    static QString nativeUriProtocol();

    /** Human-readable protocol description. */
    static QString nativeUriProtocolDescription();

    /** Base file name of linux .desktop file. */
    static QString desktopFileName();

    /** File name of the application icon (like vmsclient-default.png). */
    static QString iconFileName();

    /** Short name of the product like hdwitness. */
    static QString productNameShort();
};

} // namespace utils
} // namespace vms
} // namespace nx
