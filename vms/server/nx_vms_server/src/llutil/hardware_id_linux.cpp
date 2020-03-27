#include <vector>
#include <fstream>
#include <string>
#include <set>
#include <string.h>
#include <stdio.h>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QDebug>
#include <QtCore/QDir>

#if defined(__arm__) || defined(__aarch64__)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <arpa/inet.h>
    #include <net/if.h>
    #include <unistd.h>

    #include <QtCore/QCryptographicHash>
#endif

#include "licensing/hardware_info.h"
#include "hardware_id.h"
#include "hardware_id_p.h"
#include <nx/vms/server/root_fs.h>
#include <media_server/media_server_module.h>
#include <nx/utils/uuid.h>
#include <media_server/media_server_module.h>

namespace {

const QString kEmptyMac = lit("");
const static nx::utils::log::Tag kLogTag(lit("HardwareId"));

QString readFile(nx::vms::server::RootFileSystem* rootTool, const char* path)
{
    int fd = rootTool->open(path, QIODevice::ReadOnly);
    QFile file(path);
    if (fd < 0 || !file.open(fd, QIODevice::ReadOnly))
    {
        NX_WARNING(kLogTag, lm("[RootTool] Failed to open %1").args(path));
        return QString();
    }

    auto content = file.readAll();
    return QString::fromLatin1(content).trimmed();
}

void getMemoryInfo(
    nx::vms::server::RootFileSystem* rootTool,
    QString &partNumber, QString &serialNumber)
{
    bool result = rootTool->dmiInfo(&partNumber, &serialNumber);
    NX_VERBOSE(
        kLogTag,
        lm("[RootTool] Got memory info. Result: %1 PN: %2, SN: %3")
            .args(result, partNumber, serialNumber));
}

} // namespace {}

namespace LLUtil {

#if defined(__i686__) || defined(__x86_64__)

void findMacAddresses(QnMacAndDeviceClassList& devices) {
    QStringList classes;
    classes << "PCI" << "USB";

    QDir interfacesDir("/sys/class/net");
    QStringList interfaces = interfacesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (QString interface, interfaces) {
        QFileInfo addressFileInfo(interfacesDir, interface + "/address");
        if (!addressFileInfo.exists())
            continue;

        QFile addressFile(addressFileInfo.absoluteFilePath());
        if (!addressFile.open(QFile::ReadOnly))
            continue;

        QString mac = QString(addressFile.readAll()).trimmed().toUpper();
        if (mac.isEmpty() || mac == lit("00:00:00:00:00:00"))
            continue;

        QFileInfo subsystemFileInfo(interfacesDir, interface + "/device/subsystem");
        QString xclass = QDir(subsystemFileInfo.symLinkTarget()).dirName().toUpper();

        if (classes.contains(xclass))
            devices.push_back(QnMacAndDeviceClass(xclass, mac));
        else
            devices.push_back(QnMacAndDeviceClass("XXX", mac));
    }
}

void calcHardwareIdMap(
    QMap<QString, QString>& hardwareIdMap,
    const QnHardwareInfo& hi, const QString& boardUuid, int version)
{
    hardwareIdMap.clear();

    QString hardwareId;

    if (hi.boardID.length() || hi.boardUUID.length() || hi.biosID.length())
    {
        hardwareId = hi.boardID + boardUuid +
            hi.boardManufacturer + hi.boardProduct + hi.biosID + hi.biosManufacturer;

        if (version == 3 || version == 4) // this part differs from windows, unfortunately
            hardwareId += hi.memoryPartNumber + hi.memorySerialNumber;
    }

    if (version == 4 || version == 5)
    {
        for (const auto& nic : hi.nics)
        {
            const QString& mac = nic.mac;

            if (!mac.isEmpty())
            {
                hardwareIdMap[mac] = hardwareId + mac;
            }
        }
    }
    else // this part differs from windows, but no one knows why
    {
        hardwareIdMap[kEmptyMac] = hardwareId;
    }
}

void fillHardwareIds(
    QnMediaServerModule* serverModule,
    HardwareIdListType& hardwareIds, QnHardwareInfo& hardwareInfo)
{
    hardwareInfo.boardUUID = readFile(serverModule->rootFileSystem(), "/sys/class/dmi/id/product_uuid");
    hardwareInfo.compatibilityBoardUUID = nx::utils::changedGuidByteOrder(hardwareInfo.boardUUID);

    hardwareInfo.boardID = readFile(serverModule->rootFileSystem(), "/sys/class/dmi/id/board_serial");
    hardwareInfo.boardManufacturer = readFile(serverModule->rootFileSystem(), "/sys/class/dmi/id/board_vendor");
    hardwareInfo.boardProduct = readFile(serverModule->rootFileSystem(), "/sys/class/dmi/id/board_name");

    hardwareInfo.biosID = readFile(serverModule->rootFileSystem(), "/sys/class/dmi/id/product_serial");
    hardwareInfo.biosManufacturer = readFile(serverModule->rootFileSystem(), "/sys/class/dmi/id/bios_vendor");

    getMemoryInfo(serverModule->rootFileSystem(), hardwareInfo.memoryPartNumber, hardwareInfo.memorySerialNumber);

    findMacAddresses(hardwareInfo.nics);

    HardwareIdListForVersion macHardwareIds;

    // We start from 1 here because hwid[0] is not based on hardware
    for (int i = 1; i <= LATEST_HWID_VERSION; i++) {
        calcHardwareIds(macHardwareIds, hardwareInfo, i);
        hardwareIds << macHardwareIds;
    }
}

#elif defined(__arm__) || defined(__aarch64__)

// TODO: Use getMacFromPrimaryIF instead
bool getInterfaceMac(const char* iface, char MAC_str[13], char** host)
{
    #define HWADDR_len 6
    int s,i;
    struct ifreq ifr;
    s = socket(AF_INET, SOCK_DGRAM, 0);
    strcpy(ifr.ifr_name, iface);
    bool result = false;
    if (ioctl(s, SIOCGIFHWADDR, &ifr) != -1) {
        for (i=0; i<HWADDR_len; i++)
            sprintf(&MAC_str[i*2],"%02X",((unsigned char*)ifr.ifr_hwaddr.sa_data)[i]);
        result = true;
    }
    if((ioctl(s, SIOCGIFADDR, &ifr)) != -1) {
        const sockaddr_in* ip = (sockaddr_in*) &ifr.ifr_addr;
        if( host )
            *host = inet_ntoa(ip->sin_addr);
    }
    close(s);
    NX_DEBUG(NX_SCOPE_TAG, "%1 mac %2", iface, MAC_str);
    return result;
}

void getPrimaryMac(char MAC_str[13], char** host = nullptr)
{
    // This is for compatibility with old versions, which tried eth0 only.
    if (getInterfaceMac("eth0", MAC_str, host))
        return;

    for (const auto iface: QNetworkInterface::allInterfaces())
    {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;

        if (getInterfaceMac(iface.name().toStdString().c_str(), MAC_str, host))
            return;
    }
}

void fillHardwareIds(
    QnMediaServerModule* /*serverModule*/,
    HardwareIdListType& hardwareIds, QnHardwareInfo& hardwareInfo)
{
    char MAC_str[13];
    memset(MAC_str, 0, sizeof(MAC_str));
    getPrimaryMac(MAC_str);

    // Historically hardware id is mac + '\0'
    QByteArray hardwareId = QByteArray(MAC_str, sizeof(MAC_str));

    // when copying bytearray to string, trailing '\0' is removed
    hardwareInfo.mac = hardwareId;

    QStringList hardwareIdList = QStringList() << QString::fromUtf8(MAC_str, sizeof(MAC_str));

    HardwareIdListForVersion macHardwareIds;
    macHardwareIds << MacAndItsHardwareIds(kEmptyMac, hardwareIdList);

    for (int i = 1; i <= LATEST_HWID_VERSION; i++) {
        hardwareIds << macHardwareIds;
    }
}

void calcHardwareIdMap(
    QMap<QString, QString>& hardwareIdMap,
    const QnHardwareInfo& hi, const QString& boardUuid, int version)
{
}

#endif

} // namespace LLUtil {}
