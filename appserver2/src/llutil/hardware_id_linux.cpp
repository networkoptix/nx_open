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

#ifdef __arm__
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>

#include <QtCore/QCryptographicHash>
#endif

#include "util.h"
#include "hardware_id.h"
#include "hardware_id_pvt.h"

namespace {

QByteArray fromString(const std::string& s) {
    return QByteArray(s.data(), s.size());
}

std::string trim(const std::string& str) {
    std::string result = str;

    result.erase(0, result.find_first_not_of(" \t\n"));
    result.erase(result.find_last_not_of(" \t\n") + 1);

    return result;
}

QString read_file(const char* path) {
    std::ifstream ifs(path);
    std::string content = trim(std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>()));
    return QString::fromStdString(content);
}

void trim2(std::string& str) {
    std::string::size_type pos = str.find_last_not_of(" \n\t");

    if(pos != std::string::npos) {
        str.erase(pos + 1);
        pos = str.find_first_not_of(' ');
        if(pos != std::string::npos) str.erase(0, pos);
    } else {
        str.erase(str.begin(), str.end());
    }
}

void getMemoryInfo(QString &partNumber, QString &serialNumber) {
    static const int PARAMETER_COUNT = 2;

    const char *STRINGS[] = {
       "Part Number: ",
       "Serial Number: ",
    };

    typedef std::set<std::string> StrSet;
    StrSet values[PARAMETER_COUNT];

    FILE *fp = popen("/usr/sbin/dmidecode -t17", "r");

    if (fp == NULL)
        return;

    char buf[1024];
    while (fgets(buf, 1024, fp) != NULL) {
        for (int index = 0; index < PARAMETER_COUNT; index++) {
            char* ptr = strstr(buf, STRINGS[index]);
            if (ptr) {
                std::string value = std::string(ptr + strlen(STRINGS[index]));
                trim2(value);
                values[index].insert(value);
            }
        }
    }


    for (StrSet::const_iterator ci = values[0].begin(); ci != values[0].end(); ++ci) {
        partNumber += ci->c_str();
    }


    for (StrSet::const_iterator ci = values[1].begin(); ci != values[1].end(); ++ci) {
        serialNumber += ci->c_str();
    }


    pclose(fp);
}

} // namespace {}

namespace LLUtil {

#if defined(__i686__) || defined(__x86_64__)

void findMacAddresses(DevicesList& devices) {
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
        if (mac.isEmpty())
            continue;

        QFileInfo subsystemFileInfo(interfacesDir, interface + "/device/subsystem");
        QString xclass = QDir(subsystemFileInfo.symLinkTarget()).dirName().toUpper();

        if (classes.contains(xclass)) {
            devices.push_back(DeviceClassAndMac(xclass, mac));
        }
    }
}

static void calcHardwareId(QString &hardwareId, const HardwareInfo& hi, int version, bool guidCompatibility)
{
    if (hi.boardID.length() || hi.boardUUID.length() || hi.biosID.length()) {
        hardwareId = hi.boardID + (guidCompatibility ? hi.compatibilityBoardUUID : hi.boardUUID) + hi.boardManufacturer + hi.boardProduct + hi.biosID + hi.biosManufacturer;
        if (version == 3 || version == 4) { // this part differs from windows, unfortunately
            hardwareId += hi.memoryPartNumber + hi.memorySerialNumber;
        }
    } else {
        hardwareId.clear();
    }

    if (version == 4 && hi.mac.length() > 0)
        hardwareId += hi.mac;
}

void fillHardwareIds(QStringList& hardwareIds, QSettings *settings, HardwareInfo& hardwareInfo)
{
    hardwareInfo.boardUUID = read_file("/sys/class/dmi/id/product_uuid");
    hardwareInfo.compatibilityBoardUUID = changedGuidByteOrder(hardwareInfo.boardUUID);

    hardwareInfo.boardID = read_file("/sys/class/dmi/id/board_serial");
    hardwareInfo.boardManufacturer = read_file("/sys/class/dmi/id/board_vendor");
    hardwareInfo.boardProduct = read_file("/sys/class/dmi/id/board_name");

    hardwareInfo.biosID = read_file("/sys/class/dmi/id/product_serial");
    hardwareInfo.biosManufacturer = read_file("/sys/class/dmi/id/bios_vendor");

    getMemoryInfo(hardwareInfo.memoryPartNumber, hardwareInfo.memorySerialNumber);

    findMacAddresses(hardwareInfo.nics);
    hardwareInfo.mac = getSaveMacAddress(hardwareInfo.nics, settings);

    for (int i = 0; i < LATEST_HWID_VERSION; i++) {
        calcHardwareId(hardwareIds[i], hardwareInfo, i + 1, false);
        calcHardwareId(hardwareIds[LATEST_HWID_VERSION + i], hardwareInfo, i + 1, true);
    }
}

#elif defined(__arm__)

void mac_eth0(char  MAC_str[13], char** host)
{
    #define HWADDR_len 6
    int s,i;
    struct ifreq ifr;
    s = socket(AF_INET, SOCK_DGRAM, 0);
    strcpy(ifr.ifr_name, "eth0");
    if (ioctl(s, SIOCGIFHWADDR, &ifr) != -1) {
        for (i=0; i<HWADDR_len; i++)
            sprintf(&MAC_str[i*2],"%02X",((unsigned char*)ifr.ifr_hwaddr.sa_data)[i]);
    }
    if((ioctl(s, SIOCGIFADDR, &ifr)) != -1) {
        const sockaddr_in* ip = (sockaddr_in*) &ifr.ifr_addr;
        if( host )
            *host = inet_ntoa(ip->sin_addr);
    }
    close(s);
}


void fillHardwareIds(QStringList& hardwareIds, QSettings *settings, HardwareInfo& hardwareInfo)
{
    Q_UNUSED(settings)

    char MAC_str[13];
    memset(MAC_str, sizeof(MAC_str), 0);
    mac_eth0( MAC_str, nullptr );
    QByteArray hardwareId = QByteArray( MAC_str, sizeof(MAC_str) );
    hardwareIds.clear();
    hardwareInfo.mac = hardwareId;

    for (int i = 0; i < 2 * LATEST_HWID_VERSION; i++) {
        hardwareIds << hardwareId;
    }
}

#endif

} // namespace LLUtil {}
