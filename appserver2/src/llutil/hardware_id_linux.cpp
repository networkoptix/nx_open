#include <vector>
#include <fstream>
#include <string>
#include <set>
#include <string.h>
#include <stdio.h>

#include <QtCore/QList>
#include <QtCore/QByteArray>

#include "util.h"
#include "hardware_id.h"

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

std::string read_file(const char* path) {
    std::ifstream ifs(path);
    return trim(std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>()));
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

void getMemoryInfo(std::string& memoryInfo) {
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

    for (int i = 0; i < PARAMETER_COUNT; i++) {
        for (StrSet::const_iterator ci = values[i].begin(); ci != values[i].end(); ++ci) {
            memoryInfo += *ci;
        }
    }

    pclose(fp);
}

} // namespace {}

namespace LLUtil {

#if defined(__i686__) || defined(__x86_64__)

void fillHardwareIds(QList<QByteArray>& hardwareIds)
{
    std::string board_serial = read_file("/sys/class/dmi/id/board_serial");
    std::string board_vendor = read_file("/sys/class/dmi/id/board_vendor");
    std::string board_name = read_file("/sys/class/dmi/id/board_name");

    std::string product_uuid = read_file("/sys/class/dmi/id/product_uuid");
    std::string product_serial = read_file("/sys/class/dmi/id/product_serial");
    std::string bios_vendor = read_file("/sys/class/dmi/id/bios_vendor");

    std::string memory_info;
    getMemoryInfo(memory_info);
    QByteArray memory_info_ba = fromString(memory_info);

    QByteArray hardwareId = fromString(board_serial + product_uuid + board_vendor + board_name + product_serial + bios_vendor);
    hardwareIds << hardwareId << hardwareId << (hardwareId + memory_info_ba);
    
    changeGuidByteOrder(product_uuid);

    hardwareId = fromString(board_serial + product_uuid + board_vendor + board_name + product_serial + bios_vendor);
    hardwareIds << hardwareId << hardwareId << (hardwareId + memory_info_ba);
}

#elif defined(__arm__)

void fillHardwareIds(std::vector<std::string>& hardwareIds);
{
    QByteArray hardwareId = read_file("/proc/cpuinfo");
    hardwareIds << hardwareId << hardwareId << hardwareId << hardwareId << hardwareId << hardwareId;
}


#endif

} // namespace LLUtil {}
