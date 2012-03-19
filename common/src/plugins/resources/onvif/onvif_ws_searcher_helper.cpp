#include "onvif_ws_searcher_helper.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "utils/qtsoap/qtsoap.h"
#include "utils/network/mac_address.h"
#include "../digitalwatchdog/digital_watchdog_resource.h"

extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);
extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);

QnPlOnvifWsSearcherHelper::QnPlOnvifWsSearcherHelper()
{

}

QnPlOnvifWsSearcherHelper::~QnPlOnvifWsSearcherHelper()
{
    
}

QList<QnPlOnvifWsSearcherHelper::WSResult> QnPlOnvifWsSearcherHelper::findResources()
{
    QList<WSResult> result;

    QList<QHostAddress> localAddresses = getAllIPv4Addresses();

    foreach(QHostAddress localAddress, localAddresses)
    {
        

        QUdpSocket socket;

        bool bindSucceeded = socket.bind(localAddress, 0);
        if (!bindSucceeded)
            continue;

        QHostAddress groupAddress(QLatin1String("239.255.255.250"));
        if (!multicastJoinGroup(socket, groupAddress, localAddress)) continue;

        QString request = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\"><s:Header><a:Action s:mustUnderstand=\"1\">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</a:Action><a:MessageID>uuid:05d8735f-098e-4157-85f9-99fa40ab2855</a:MessageID><a:ReplyTo><a:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address></a:ReplyTo><a:To s:mustUnderstand=\"1\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:To></s:Header><s:Body><Probe xmlns=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\"><d:Types xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:dp0=\"http://www.onvif.org/ver10/network/wsdl\">dp0:NetworkVideoTransmitter</d:Types></Probe></s:Body></s:Envelope>";
        QByteArray requestDatagram = request.toUtf8();
        

        //QtSoapMessage message;
        //message.setMethod("getTemperature", "http://weather.example.com/temperature");
        //message.addMethodArgument("city", "Oslo", 5);
        // Get the SOAP message as an XML string.
        //QString xml = message.toXmlString();
        //QByteArray requestDatagram = xml.toUtf8();

        socket.writeDatagram(requestDatagram.data(), requestDatagram.size(), groupAddress, 3702);

        QnSleep::msleep(150);
        while(socket.hasPendingDatagrams())
        {
            QByteArray reply;
            reply.resize(socket.pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;

            socket.readDatagram(reply.data(), reply.size(),	&sender, &senderPort);

            WSResult res = parseReply(reply);
            if (res.mac!="")
            {
                res.ip = sender.toString();
                res.disc_ip = localAddress.toString();
                result.push_back(res);
            }

        }
        multicastLeaveGroup(socket, groupAddress);
    }

    
    

    return result;
}


//=========================================================================
QnPlOnvifWsSearcherHelper::WSResult QnPlOnvifWsSearcherHelper::parseReply(QByteArray& datagram)
{

    WSResult result;
    QString reply = QString(datagram);
    if (!reply.contains("Digital") || !reply.contains("Watchdog"))
    {
        return result;
    }




    QDomDocument doc;
    if (!doc.setContent(datagram))
        return result;

    QDomElement root = doc.documentElement();

    QDomNodeList lst = doc.elementsByTagName(QLatin1String("wsa:EndpointReference")); 
    if (lst.size()<1)
    {
        return result;
    }


    QString mac;
    for (int i = 0; i < lst.size(); ++i)
    {
        QDomNode node = lst.at(i);
        QDomElement addr = node.firstChildElement(QLatin1String("wsa:Address"));
        if (addr.isNull())
            continue;

        QString addrText = addr.text();
        QStringList lst = addrText.split("-", QString::SkipEmptyParts);
        if (lst.size()<2)
            continue;

        mac = lst.back().toUpper();

        if (mac.length()!=12)
            continue;

        mac = QnMacAddress(mac).toString();

        break;
    }





    //int index = reply.indexOf("EndpointReference");
    //if (index<0)

    result.name = QnPlWatchDogResource::MANUFACTURE;
    result.manufacture = QnPlWatchDogResource::MANUFACTURE;
    result.mac = mac;


    return result;
}