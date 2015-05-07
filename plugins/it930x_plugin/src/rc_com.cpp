#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstring>

#include "rc_com.h"

namespace ite
{
    ComPort::ComPort(const char * devName, unsigned baudrate)
    :   cport_(-1),
        name_(devName)
    {
        baudrate_ = checkBaudrate(baudrate);
        cport_ = open(name_, O_RDWR|O_NONBLOCK);
        if (cport_ == -1)
            return;

        int ret = tcgetattr(cport_, &old_port_settings);
        if (ret == -1) {
            close(cport_);
            cport_ = -1;
            return;
        }

        memset(&new_port_settings, 0, sizeof(new_port_settings));

        new_port_settings.c_cflag = baudrate_ | CS8 | CLOCAL | CREAD;
        new_port_settings.c_iflag = IGNPAR;
        new_port_settings.c_oflag = 0;
        new_port_settings.c_lflag = 0;
        new_port_settings.c_cc[VMIN] = 100;      /* block untill n bytes are received */
        new_port_settings.c_cc[VTIME] = 100;     /* block untill a timer expires (n * 100 mSec.) */

        ret = tcsetattr(cport_, TCSANOW, &new_port_settings);
        if (ret == -1) {
            close(cport_);
            cport_ = -1;
            return;
        }
    }

    ComPort::~ComPort()
    {
        if (cport_ >= 0)
            close(cport_);
        tcsetattr(cport_, TCSANOW, &old_port_settings);
    }

    int ComPort::poll(unsigned char * buf, int size)
    {
        static const int COM_MAX_SIZE = 4096;

        if (size > COM_MAX_SIZE)
            size = COM_MAX_SIZE;
        return read(cport_, buf, size);
    }

    int ComPort::send(const unsigned char * buf, int size)
    {
        return write(cport_, buf, size);
    }

    int ComPort::checkBaudrate(unsigned baudrate)
    {
        switch (baudrate)
        {
            case      50: return B50;
            case      75: return B75;
            case     110: return B110;
            case     134: return B134;
            case     150: return B150;
            case     200: return B200;
            case     300: return B300;
            case     600: return B600;
            case    1200: return B1200;
            case    1800: return B1800;
            case    2400: return B2400;
            case    4800: return B4800;
            case    9600: return B9600;
            case   19200: return B19200;
            case   38400: return B38400;
            case   57600: return B57600;
            case  115200: return B115200;
            case  230400: return B230400;
            case  460800: return B460800;
            case  500000: return B500000;
            case  576000: return B576000;
            case  921600: return B921600;
            case 1000000: return B1000000;
            default:
                return B9600;
        }
    }

    ComPort * ComPort::Instance = nullptr;
}

unsigned User_returnChannelBusTx(unsigned bufferLength, const unsigned char * buffer, int * txLen)
{
    *txLen = ite::ComPort::Instance->send(buffer, bufferLength);
    return 0;
}

unsigned User_returnChannelBusRx(unsigned bufferLength, unsigned char * buffer, int * rxLen)
{
    *rxLen = ite::ComPort::Instance->poll(buffer, bufferLength);
    return 0;
}

///

#if 0
using namespace ret_chan;

int main()
{
    try
    {
        ComPort port("/dev/tnt0", RETURN_CHANNEL_BAUDRATE);
        pComPort = &port;

        ret_chan::RCShell shell;
        shell.startRcvThread();

        sleep(1);

        std::vector<IDsLink> devLinks;
        shell.getDevIDs(devLinks);

        if (devLinks.empty())
            throw "No devices";

        for (unsigned i = 0; i < devLinks.size(); ++i)
        {
            ret_chan::DeviceInfoPtr devInfo = shell.device(devLinks[i]);
            if (!devInfo || !devInfo->isOn())
                continue;

            /*
            devInfo->getTransmissionParameters();
            bool ok = devInfo->wait(DeviceInfo::SEND_WAIT_TIME);
            if (!ok)
                throw "getTransmissionParameters";

            devInfo->getTransmissionParameterCapabilities();
            devInfo->getAdvanceOptions();
            devInfo->getCapabilities();
            devInfo->getDeviceInformation();
            devInfo->getHostname();
            devInfo->getSystemDateAndTime();

            // deviceInfo->imageConfig.videoSourceToken
            devInfo->getImagingSettings();

            devInfo->getProfiles();
            devInfo->getVideoSources();
            devInfo->getVideoSourceConfigurations();
            //devInfo->getNumberOfVideoEncoderInstances();
            devInfo->getVideoEncoderConfigurations();
            */

            devInfo->setEncoderCfg(1, 1000, 10);
            devInfo->setEncoderCfg(1, 2000, 30);
            devInfo->getVideoEncoderConfigurations();

#if 0
            // deviceInfo->focusStatusInfo.videoSourceToken
            devInfo->imgGetStatus();

            // deviceInfo->imageConfigOption.videoSourceToken
            devInfo->imgGetOptions();

            devInfo->getVideoEncoderConfigurations();

            // retCode: 253
            devInfo->getVideoOSDConfiguration();
#endif
            devInfo->getSupportedRules();

            //devInfo->systemReboot();
        }

        shell.printDevices();
        shell.debugInfo().print();
    }
    catch (const char * msg)
    {
        if (msg)
            printf("%s\n", msg);
    }

    return 0;
}
#endif
