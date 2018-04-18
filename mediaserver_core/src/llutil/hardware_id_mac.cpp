#include <string>

#ifndef Q_OS_IOS
#include <IOKit/IOKitLib.h>
#endif

#include <cassert>

#include <QtCore/QStringList>

#include <nx/utils/log/assert.h>

#include "licensing/hardware_info.h"
#include "hardware_id.h"
#include "hardware_id_p.h"

namespace {

inline std::string trim(const std::string& str)
{
    std::string result = str;

    result.erase(0, result.find_first_not_of(" \t\n"));
    result.erase(result.find_last_not_of(" \t\n") + 1);

    return result;
}

}

QString getHardwareId(int version, bool guidCompatibility)
{
    NX_ASSERT( false );
    return QString();
}

QStringList getMainHardwareIds(int guidCompatibility)
{
    NX_ASSERT( false );
    return QStringList();
}

QStringList getCompatibleHardwareIds(int guidCompatibility)
{
    NX_ASSERT( false );
    return QStringList();
}


namespace LLUtil
{
void fillHardwareIds(QStringList& hardwareIds, QSettings *settings, QnHardwareInfo& hardwareInfo)
{
    Q_UNUSED(settings)
    #define MAX_HWID_SIZE 1024

#ifndef Q_OS_IOS

    char buf[MAX_HWID_SIZE];

    io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
    CFStringRef uuidCf = (CFStringRef) IORegistryEntryCreateCFProperty(ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
    IOObjectRelease(ioRegistryRoot);
    CFStringGetCString(uuidCf, buf, MAX_HWID_SIZE, kCFStringEncodingMacRoman);
    CFRelease(uuidCf);

    hardwareIds[0] = hardwareIds[1] = hardwareIds[2] = hardwareIds[3] = hardwareIds[4] = hardwareIds[5] = QLatin1String(buf);

#endif
}

void fillHardwareIds(QList<QList<LLUtil::MacAndItsHardwareIds> >&, QnHardwareInfo&)
{
    // Unused on macosx, but has to be present to linkage to succeed.
    NX_CRITICAL(false);
}

void calcHardwareIdMap(QMap<QString, QString>&, const QnHardwareInfo&, int, bool)
{
    // Unused on macosx, but has to be present to linkage to succeed.
    NX_CRITICAL(false);
}

}
