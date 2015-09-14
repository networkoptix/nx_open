#ifndef _HARDWARE_ID_PVT_H_
#define _HARDWARE_ID_PVT_H_

namespace LLUtil {
class HardwareIdError : public std::exception {
public:
    HardwareIdError(const std::string& msg_): msg(msg_) {}
    ~HardwareIdError() throw () {}

    virtual const char* what() const throw () { return msg.c_str(); }

private:
    std::string msg;
};

// TODO: Hide these implementation details
struct DeviceClassAndMac {
    DeviceClassAndMac() {}

    DeviceClassAndMac(const QString &_class, const QString &_mac)
        : xclass(_class),
          mac(_mac)
    {}

    QString xclass;
    QString mac;
};

typedef std::vector<DeviceClassAndMac> DevicesList;

struct HardwareInfo  {
    QString boardID;
    QString boardUUID;
    QString compatibilityBoardUUID;
    QString boardManufacturer;
    QString boardProduct;

    QString biosID;
    QString biosManufacturer;

    QString memoryPartNumber;
    QString memorySerialNumber;

    DevicesList nics;
    QString mac;

    void write(QJsonObject &json) const;
};

QString getSaveMacAddress(DevicesList devices, QSettings *settings);
}

#endif // _HARDWARE_ID_PVT_H_