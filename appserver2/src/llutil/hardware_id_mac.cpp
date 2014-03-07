#include <string>
#include <IOKit/IOKitLib.h>

#include "hardware_id.h"

namespace {

inline std::string trim(const std::string& str)
{
    std::string result = str;

    result.erase(0, result.find_first_not_of(" \t\n"));
    result.erase(result.find_last_not_of(" \t\n") + 1);

    return result;
}

}

void LLUtil::fillHardwareIds(std::vector<std::string>& hardwareIds);
{
    #define MAX_HWID_SIZE 1024

    char buf[MAX_HWID_SIZE];

    io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
    CFStringRef uuidCf = (CFStringRef) IORegistryEntryCreateCFProperty(ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
    IOObjectRelease(ioRegistryRoot);
    CFStringGetCString(uuidCf, buf, MAX_HWID_SIZE, kCFStringEncodingMacRoman);
    CFRelease(uuidCf);

    hardwareIds[0] = hardwareIds[1] = hardwareIds[2] = hardwareIds[3] = hardwareIds[4] = hardwareIds[5] = buf;
}
