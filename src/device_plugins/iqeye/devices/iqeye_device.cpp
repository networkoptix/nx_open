#include "iqeye_device.h"
#include "../streamreader/iqeye_sp_h264.h"
#include "../streamreader/iqeye_sp_mjpeg.h"
#include "network/nettools.h"
#include "network/mdns.h"
#include "base/sleep.h"

// These functions added temporary as in Qt 4.8 they are already in QUdpSocket
bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress)
{
    struct ip_mreq imr;

    memset(&imr, 0, sizeof(imr));

    imr.imr_multiaddr.s_addr = htonl(groupAddress.toIPv4Address());
    imr.imr_interface.s_addr = htonl(localAddress.toIPv4Address());

    int res = setsockopt(udpSocket.socketDescriptor(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&imr, sizeof(struct ip_mreq));
    if (res == -1)
        return false;

    return true;
}

bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress)
{
    struct ip_mreq imr;

    imr.imr_multiaddr.s_addr = htonl(groupAddress.toIPv4Address());
    imr.imr_interface.s_addr = INADDR_ANY;

    int res = setsockopt(udpSocket.socketDescriptor(), IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&imr, sizeof(struct ip_mreq));
    if (res == -1)
        return false;

    return true;
}

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
        return new CLIQEyeH264treamreader(this);
    return new CLIQEyeMJPEGtreamreader(this);
}

bool CLIQEyeDevice::unknownDevice() const
{
    return false;
}

CLNetworkDevice* CLIQEyeDevice::updateDevice()
{
    return 0;
}

void CLIQEyeDevice::findDevices(CLDeviceList& result)
{
    QList<QHostAddress> localAddresses = getAllIPv4Addresses();

    CL_LOG(cl_logDEBUG1)
    {
        QString log;
        QTextStream(&log) << "IQEyeDeviceServer::findDevices  found " << localAddresses.size() << " adapter(s) with IPV4";
        cl_log.log(log, cl_logDEBUG1);

        for (int i = 0; i < localAddresses.size();++i)
        {
            QString slog;
            QTextStream(&slog) << localAddresses.at(i).toString();
            cl_log.log(slog, cl_logDEBUG1);
        }
    }

    foreach(QHostAddress localAddress, localAddresses)
    {
        QHostAddress groupAddress(QLatin1String("224.0.0.251"));
        quint16 MDNS_PORT = 5353;

        std::string localAddressString = localAddress.toString().toStdString();
        std::string groupAddressString = groupAddress.toString().toStdString();

        QUdpSocket udpSocket;
        udpSocket.bind(localAddress, MDNS_PORT, QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);
        if (!multicastJoinGroup(udpSocket, groupAddress, localAddress))
            continue;

        MDNSPacket request;
        MDNSPacket response;

        request.addQuery();

        QByteArray datagram;
        request.toDatagram(datagram);

        udpSocket.writeDatagram(datagram.data(), datagram.size(), groupAddress, MDNS_PORT);

        QTime time;
        time.start();

        while(time.elapsed() < 150)
        {
            while (udpSocket.hasPendingDatagrams())
            {
                QByteArray responseData;
                responseData.resize(udpSocket.pendingDatagramSize());

                QHostAddress sender;
                quint16 senderPort;

                udpSocket.readDatagram(responseData.data(), responseData.size(),	&sender, &senderPort);
                //cl_log.log(cl_logALWAYS, "size: %d\n", responseData.size());
                if (senderPort != MDNS_PORT || sender == localAddress)
                    continue;

                QString ip = sender.toString();
                QString smac;
                QString name;

                int iqpos = responseData.indexOf("IQ");

                if (iqpos == -1)
                    continue;

                int macpos = responseData.indexOf('-', iqpos);

                for (int i = iqpos; i < macpos; i++)
                {
                    name += QLatin1Char(responseData[i]);
                }

                if (macpos != -1)
                {
                    macpos++;

                    for (int i = 0; i < 12; i++)
                    {
                        if (i > 0 && i % 2 == 0)
                            smac += QLatin1Char('-');

                        smac += QLatin1Char(responseData[macpos + i]);
                    }
                }

                //response.fromDatagram(responseData);

                smac = smac.toUpper();
                if (result.find(smac) != result.end())
                    continue; // already found;

                // in any case let's HTTP do it's job at very end of discovery

                CLIQEyeDevice* device = new CLIQEyeDevice();
                device->setName(name);

                if (device==0)
                    continue;

                device->setIP(sender, false);
                device->setMAC(smac);
                device->setUniqueId(smac);

                CLDeviceStatus dh;
                device->m_local_adssr = localAddress;
                device->setStatus(dh);

                result[smac] = device;

            }

            CLSleep::msleep(2); // to avoid 100% cpu usage
        }

         multicastLeaveGroup(udpSocket, groupAddress);
    }
}
