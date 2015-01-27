#ifndef _HARDWARE_ID_H_
#define _HARDWARE_ID_H_

#include <stdexcept>
#include <string>

namespace LLUtil {

class HardwareIdError : public std::exception {
public:
    HardwareIdError(const std::string& msg_): msg(msg_) {}
    ~HardwareIdError() throw () {}

    virtual const char* what() const throw () { return msg.c_str(); }

private:
    std::string msg;
};

const int LATEST_HWID_VERSION = 3;

QByteArray getHardwareId(int version, bool guidCompatibility);
QList<QByteArray> getMainHardwareIds(int guidCompatibility);
QList<QByteArray> getCompatibleHardwareIds(int guidCompatibility);

}

#endif // _HARDWARE_ID_H_
