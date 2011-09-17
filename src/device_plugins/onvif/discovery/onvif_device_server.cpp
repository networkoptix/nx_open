#include "onvif_device_server.h"
#include "network/nettools.h"
#include "network/mdns.h"
#include "base/sleep.h"
#include "../../iqeye/devices/iqeye_device.h"
#include "../../axis/devices/axis_device.h"


quint16 MDNS_PORT = 5353;

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




OnvifDeviceServer::OnvifDeviceServer()
{

}

OnvifDeviceServer::~OnvifDeviceServer()
{

};

OnvifDeviceServer& OnvifDeviceServer::instance()
{
    static OnvifDeviceServer inst;
    return inst;
}

bool OnvifDeviceServer::isProxy() const
{
    return false;
}

QString OnvifDeviceServer::name() const
{
    return QLatin1String("ONVIF");
}


CLDeviceList OnvifDeviceServer::findDevices()
{


    CLDeviceList result;

    QList<QHostAddress> localAddresses = getAllIPv4Addresses();

    CL_LOG(cl_logDEBUG1)
    {
        QString log;
        QTextStream(&log) << "OnvifDeviceServer::findDevices  found " << localAddresses.size() << " adapter(s) with IPV4";
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

        QUdpSocket sendSocket, recvSocket;

        bool bindSucceeded = sendSocket.bind(localAddress, 0);
        if (!bindSucceeded)
            continue;

        bindSucceeded = recvSocket.bind(QHostAddress::Any, MDNS_PORT, QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);
        
        if (!bindSucceeded)
            continue;

        if (!multicastJoinGroup(recvSocket, groupAddress, localAddress))      continue;

        MDNSPacket request;
        MDNSPacket response;

        request.addQuery();

        QByteArray datagram;
        request.toDatagram(datagram);

        sendSocket.writeDatagram(datagram.data(), datagram.size(), groupAddress, MDNS_PORT);

        QTime time;
        time.start();

        while(time.elapsed() < 200)
        {
            checkSocket(recvSocket, result, localAddress);
            checkSocket(sendSocket, result, localAddress);
            CLSleep::msleep(2); // to avoid 100% cpu usage
      
        }

        multicastLeaveGroup(recvSocket, groupAddress);
    }


    return result;
}


void OnvifDeviceServer::checkSocket(QUdpSocket& sock, CLDeviceList& result, QHostAddress localAddress)
{
    while (sock.hasPendingDatagrams())
    {
        QByteArray responseData;
        responseData.resize(sock.pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;

        sock.readDatagram(responseData.data(), responseData.size(),	&sender, &senderPort);
        //cl_log.log(cl_logALWAYS, "size: %d\n", responseData.size());
        if (senderPort != MDNS_PORT)// || sender == localAddress)
            senderPort = senderPort;
        //continue;

        CLNetworkDevice* device = processPacket(result, responseData);

        if (device==0)
            continue;


        device->setIP(sender, false);
        device->setLocalAddr(localAddress);


        result[device->getMAC()] = device;

    }

}

CLNetworkDevice* OnvifDeviceServer::processPacket(CLDeviceList& result, QByteArray& responseData)
{
    char* Mnufactures[] = {"IQ", "AXIS"};
    int Mnufactures_SIZE = sizeof(Mnufactures)/sizeof(char*);

    QString smac;
    QString name;
    QString manufacture;

    int iqpos = -1;

    for (int i = 0; i < Mnufactures_SIZE; ++i)
    {
        manufacture = Mnufactures[i];
        iqpos = responseData.indexOf(manufacture);
        if (iqpos>=0)
            break;
    }

    if (iqpos<0)
        return 0;

    int macpos = responseData.indexOf('-', iqpos);
    if (macpos < 0)
        return 0;

    for (int i = iqpos; i < macpos; i++)
    {
        name += QLatin1Char(responseData[i]);
    }

    name.replace(QString(" "), QString()); // remove spaces
    name.replace(QString("\t"), QString()); // remove tabs

    if (macpos+12 > responseData.size())
        return 0;


    macpos++; // -

    while(responseData.at(macpos)==' ')
        ++macpos;


    if (macpos+12 > responseData.size())
        return 0;



    for (int i = 0; i < 12; i++)
    {
        if (i > 0 && i % 2 == 0)
            smac += QLatin1Char('-');

        smac += QLatin1Char(responseData[macpos + i]);
    }


    //response.fromDatagram(responseData);

    smac = smac.toUpper();
    if (result.find(smac) != result.end())
        return 0; // already found;

    // in any case let's HTTP do it's job at very end of discovery

    CLNetworkDevice* device = 0;

    if (manufacture == "IQ")
        device = new CLIQEyeDevice();
    else if (manufacture == "AXIS")
        device = new CLAxisDevice();

    if (device==0)
        return 0;


    device->setName(name);


    device->setMAC(smac);
    device->setUniqueId(smac);

    CLDeviceStatus dh;
    device->setStatus(dh);

    return device;


}