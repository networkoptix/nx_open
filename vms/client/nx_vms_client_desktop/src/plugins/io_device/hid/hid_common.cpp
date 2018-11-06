#include "hid_common.h"

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace hid {

bool isReservedUsagePage(quint16 pageCode)
{
    return pageCode == 0x0e 
        || (pageCode >= 0x11 && pageCode <= 0x13)
        || (pageCode >= 0x15 && pageCode <= 0x3f)
        || (pageCode >= 0x41 && pageCode <= 0x7f)
        || (pageCode >= 0x88 && pageCode <= 0x8b)
        || (pageCode >= 0x92 && pageCode <= 0xfeff);
}

bool isVendorDefinedUsagePage(quint16 pageCode)
{
    return pageCode >= 0xff00 && pageCode <= 0xffff;
}

bool isPowerUsagePage(quint16 pageCode)
{
    return pageCode >= 0x84 && pageCode <= 0x87;
}

bool isMonitorUsagePage(quint16 pageCode)
{
    return pageCode >= 0x80 && pageCode <=0x83;
}

} // namespace hid
} // namespace io_device
} // namespace plugins
} // namespace client 
} // namespace nx