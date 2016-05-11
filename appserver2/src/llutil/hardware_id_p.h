#ifndef _HARDWARE_ID_PVT_H_
#define _HARDWARE_ID_PVT_H_

#include "licensing/hardware_info.h"

// HWID_Version -> {MAC -> [HWID]}
typedef QVector<QMap<QString, QStringList>> HardwareIdListType;

namespace LLUtil {
class HardwareIdError : public std::exception {
public:
    HardwareIdError(const std::string& msg_): msg(msg_) {}
    ~HardwareIdError() throw () {}

    virtual const char* what() const throw () { return msg.c_str(); }

private:
    std::string msg;
};

QStringList getMacAddressList(const QnMacAndDeviceClassList& devices);
QString saveMac(const QStringList& macs, QSettings *settings);
}

#endif // _HARDWARE_ID_PVT_H_