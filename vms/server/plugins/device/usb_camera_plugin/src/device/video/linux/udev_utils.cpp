#ifdef __linux__

#include "udev_utils.h"

#include <libudev.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <iostream>

namespace nx {
namespace usb_cam {
namespace device {
namespace video {
namespace detail {

namespace {

static constexpr char kSubSystem[] = "video4linux";
static constexpr char kIdSerialKey[] = "ID_SERIAL";

struct Udev
{
    struct udev * udev;

    Udev():
        udev(udev_new())
    {
    }

    ~Udev()
    {
        if (udev)
            udev_unref(udev);
    }
};

struct UdevEnumerate
{
    struct udev_enumerate* udevEnumerate;

    UdevEnumerate(struct udev * udev):
        udevEnumerate(udev_enumerate_new(udev))
    {
    }

    ~UdevEnumerate()
    {
        if (udevEnumerate)
            udev_enumerate_unref(udevEnumerate);
    }

    void addMatchSubsystem(const char * subsystem)
    {
        udev_enumerate_add_match_subsystem(udevEnumerate, subsystem);
    }

    int scanDevices()
    {
        return udev_enumerate_scan_devices(udevEnumerate);
    }

    struct udev_list_entry * getListEntry()
    {
        return udev_enumerate_get_list_entry(udevEnumerate);
    }

    const char * getListEntryName(struct udev_list_entry * udev_list_entry)
    {
        return udev_list_entry_get_name(udev_list_entry);
    }
};

struct UdevDevice
{
    struct udev_device * udevDevice;

    UdevDevice(struct udev * udev, const char * sysPath):
        udevDevice(udev_device_new_from_syspath(udev, sysPath))
    {
    }

    ~UdevDevice()
    {
        if (udevDevice)
            udev_device_unref(udevDevice);
    }

    const char * getDevNode()
    {
        return udev_device_get_devnode(udevDevice);
    }

    const char * getPropertyValue(const char * key)
    {
        return udev_device_get_property_value(udevDevice, key);
    }
};

} // namespace

std::string getDeviceUniqueId(const std::string& devicePath)
{
    Udev udev;
    UdevEnumerate enumerator(udev.udev);

    enumerator.addMatchSubsystem(kSubSystem);
    enumerator.scanDevices();

    struct udev_list_entry * firstEntry = enumerator.getListEntry();
    struct udev_list_entry * currentEntry;

    udev_list_entry_foreach(currentEntry, firstEntry)
    {
        UdevDevice device(udev.udev, enumerator.getListEntryName(currentEntry));
        if (!device.udevDevice)
            continue;

        if (strcmp(devicePath.c_str(), device.getDevNode()) != 0)
            continue;

        const char * property = device.getPropertyValue(kIdSerialKey);
        return property ? property : std::string();
    }

    return std::string();
}

} // namespace detail
} // namespace video
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif //__linux__
