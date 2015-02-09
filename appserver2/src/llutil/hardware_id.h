#ifndef _HARDWARE_ID_H_
#define _HARDWARE_ID_H_

#include <stdexcept>
#include <string>

class QSettings;

namespace LLUtil {

class HardwareIdError : public std::exception {
public:
    HardwareIdError(const std::string& msg_): msg(msg_) {}
    ~HardwareIdError() throw () {}

    virtual const char* what() const throw () { return msg.c_str(); }

private:
    std::string msg;
};

const int LATEST_HWID_VERSION = 4;

QByteArray getHardwareId(int version, bool guidCompatibility, QSettings *settings);
QList<QByteArray> getMainHardwareIds(int guidCompatibility, QSettings *settings);
QList<QByteArray> getCompatibleHardwareIds(int guidCompatibility, QSettings *settings);

}

#endif // _HARDWARE_ID_H_
