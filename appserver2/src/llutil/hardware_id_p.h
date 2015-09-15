#ifndef _HARDWARE_ID_PVT_H_
#define _HARDWARE_ID_PVT_H_

#include "licensing/hardware_info.h"

namespace LLUtil {
class HardwareIdError : public std::exception {
public:
    HardwareIdError(const std::string& msg_): msg(msg_) {}
    ~HardwareIdError() throw () {}

    virtual const char* what() const throw () { return msg.c_str(); }

private:
    std::string msg;
};

QString getSaveMacAddress(const QnMacAndDeviceClassList& devices, QSettings *settings);
}

#endif // _HARDWARE_ID_PVT_H_