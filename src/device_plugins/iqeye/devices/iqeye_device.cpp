#include "iqeye_device.h"
#include "network/nettools.h"
#include "network/mdns.h"
#include "base/sleep.h"
#include "../../onvif/dataproviders/sp_mjpeg.h"
#include "../../onvif/dataproviders/sp_h264.h"

// These functions added temporary as in Qt 4.8 they are already in QUdpSocket
extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);


extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);


CLIQEyeDevice::CLIQEyeDevice()
{

}

CLIQEyeDevice::~CLIQEyeDevice()
{

}


CLDevice::DeviceType CLIQEyeDevice::getDeviceType() const
{
    return VIDEODEVICE;
}

QString CLIQEyeDevice::toString() const
{
    return QLatin1String("live iqeye ") + getName() + QLatin1Char(' ') + getUniqueId();
}

CLStreamreader* CLIQEyeDevice::getDeviceStreamConnection()
{
    //IQ732N   IQ732S     IQ832N   IQ832S   IQD30S   IQD31S  IQD32S  IQM30S  IQM31S  IQM32S
    QString name = getName();
    if (name == QLatin1String("IQA35") ||
        name == QLatin1String("IQA33N") ||
        name == QLatin1String("IQA32N") ||
        name == QLatin1String("IQA31") ||
        name == QLatin1String("IQ732N") ||
        name == QLatin1String("IQ732S") ||
        name == QLatin1String("IQ832N") ||
        name == QLatin1String("IQ832S") ||
        name == QLatin1String("IQD30S") ||
        name == QLatin1String("IQD31S") ||
        name == QLatin1String("IQD32S") ||
        name == QLatin1String("IQM30S") ||
        name == QLatin1String("IQM31S") ||
        name == QLatin1String("IQM32S"))
        return new RTPH264Streamreader(this);
    return new MJPEGtreamreader(this, "now.jpg?snap=spush?dummy=1305868336917");
}

bool CLIQEyeDevice::unknownDevice() const
{
    return false;
}

CLNetworkDevice* CLIQEyeDevice::updateDevice()
{
    return 0;
}



