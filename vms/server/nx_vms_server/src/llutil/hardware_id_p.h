#pragma once

#include <QtCore/QSettings>
#include "licensing/hardware_info.h"

namespace LLUtil {

struct MacAndItsHardwareIds
{
    MacAndItsHardwareIds(const QString& mac, const QStringList& hwids)
        : mac(mac),
        hwids(hwids)
    {}

    QString mac;
    QStringList hwids;
};

// HWID_Version -> [MAC, [HWID]]
typedef QList<MacAndItsHardwareIds> HardwareIdListForVersion;
typedef QList<HardwareIdListForVersion> HardwareIdListType;

class HardwareIdError : public std::exception
{
public:
    HardwareIdError(const std::string& msg_) : msg(msg_) {}
    ~HardwareIdError() throw () {}

    virtual const char* what() const throw () { return msg.c_str(); }

private:
    std::string msg;
};

void calcHardwareIds(HardwareIdListForVersion& macHardwareIds, const QnHardwareInfo& hardwareInfo, int version);
void calcHardwareIdMap(QMap<QString, QString>& hardwareIdMap, const QnHardwareInfo& hi, int version, bool guidCompatibility);

QStringList getMacAddressList(const QnMacAndDeviceClassList& devices);
QString saveMac(const QStringList& macs, QSettings* settings);
}
