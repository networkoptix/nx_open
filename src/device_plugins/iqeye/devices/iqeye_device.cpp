#include "iqeye_device.h"
#include "../streamreader/iqeye_sp_h264.h"
#include "../streamreader/iqeye_sp_mjpeg.h"
#include "network/nettools.h"
#include "network/mdns.h"
#include "base/sleep.h"

// These functions added temporary as in Qt 4.8 they are already in QUdpSocket
void multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress)
{
    struct ip_mreq imr;

    memset(&imr, 0, sizeof(imr));

    imr.imr_multiaddr.s_addr = htonl(groupAddress.toIPv4Address());
    imr.imr_interface.s_addr = htonl(localAddress.toIPv4Address()); //htonl(INADDR_ANY);

    int res = setsockopt(udpSocket.socketDescriptor(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&imr, sizeof(struct ip_mreq));
    if (res == -1)
    {
        cl_log.log("Unable to join multicast group", cl_logERROR);
    }
}

void multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress)
{
    struct ip_mreq imr;

    imr.imr_multiaddr.s_addr = htonl(groupAddress.toIPv4Address());
    imr.imr_interface.s_addr = INADDR_ANY;

    int res = setsockopt(udpSocket.socketDescriptor(), IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&imr, sizeof(struct ip_mreq));
    if (res == -1)
    {
        cl_log.log("Unable to join multicast group", cl_logERROR);
    }
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
    return QString("live iqeye ") + getUniqueId();
}

CLStreamreader* CLIQEyeDevice::getDeviceStreamConnection()
{
    if (getName()=="IQ042S")
        return new CLIQEyeH264treamreader(this);
    else
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
        QHostAddress groupAddress("224.0.0.251");
        quint16 MDNS_PORT = 5353;

        std::string localAddressString = localAddress.toString().toStdString();
        std::string groupAddressString = groupAddress.toString().toStdString();

//        UDPSocket udpSocket;

        //try
        //{
        //    udpSocket.setLocalAddressAndPort(localAddressString, MDNS_PORT);
        //}
        //catch (...)
        //{
        //    continue;
        //}
        

        // udpSocket.joinGroup(groupAddressString);

        QUdpSocket udpSocket;
        udpSocket.bind(localAddress, MDNS_PORT, QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);
        multicastJoinGroup(udpSocket, groupAddress, localAddress);

        MDNSPacket request;
        MDNSPacket response;

        request.addQuery();

        QByteArray datagram;
        request.toDatagram(datagram);

        udpSocket.writeDatagram(datagram.data(), datagram.size(), groupAddress, MDNS_PORT);
        // udpSocket.setDestAddr("224.0.0.251", 5353);
        // udpSocket.send(datagram.data(), datagram.size());

        QTime time;
        time.start();

        char buffer[1024];
        int size = 1024;
        while(time.elapsed() < 150)
        {
            while (udpSocket.hasPendingDatagrams()) 
            //while (udpSocket.recv(buffer, 1024)) 
            {
                QByteArray responseData; //(buffer, size);
                responseData.resize(udpSocket.pendingDatagramSize());

                QHostAddress sender;
                quint16 senderPort;

                udpSocket.readDatagram(responseData.data(), responseData.size(),	&sender, &senderPort);
                //std::string sourceAddr;
                //unsigned short sourcePort;
                //udpSocket.recvFrom(buffer, size, sourceAddr, sourcePort);
                cl_log.log(cl_logALWAYS, "size: %d\n", responseData.size());
                if (senderPort != MDNS_PORT || sender == localAddress)
                    continue;

                QString ip = sender.toString();
                QString smac;
                QString name;

                int iqpos = responseData.indexOf("IQ");

                if (iqpos != -1)
                {
                    int macpos = responseData.indexOf('-', iqpos);

                    for (int i = iqpos; i < macpos; i++)
                    {
                        name += responseData[i];
                    }

                    if (macpos != -1)
                    {
                        macpos++;

                        for (int i = 0; i < 12; i++)
                        {
                            if (i > 0 && i % 2 == 0)
                                smac += "-";

                            smac += responseData[macpos + i];
                        }
                    }
                }

                //response.fromDatagram(responseData);

                smac = smac.toUpper();
                if (result.find(smac) != result.end())
                    continue; // already found;

                // in any case let's HTTP do it's job at very end of discovery 

                CLIQEyeDevice* device = new CLIQEyeDevice();
                device->setName("IQUNKNOWN");

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

//        udpSocket.leaveGroup(groupAddress.toString().toStdString());
         multicastLeaveGroup(udpSocket, groupAddress);
    }
}
