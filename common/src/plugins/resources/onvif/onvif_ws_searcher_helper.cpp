#include "onvif_ws_searcher_helper.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"


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
        //if (!multicastJoinGroup(recvSocket, groupAddress, localAddress))      continue;

        QString request = "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\"><s:Header><a:Action s:mustUnderstand=\"1\">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</a:Action><a:MessageID>uuid:05d8735f-098e-4157-85f9-99fa40ab2855</a:MessageID><a:ReplyTo><a:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address></a:ReplyTo><a:To s:mustUnderstand=\"1\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:To></s:Header><s:Body><Probe xmlns=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\"><d:Types xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:dp0=\"http://www.onvif.org/ver10/network/wsdl\">dp0:NetworkVideoTransmitter</d:Types></Probe></s:Body></s:Envelope>";
        QByteArray requestDatagram = request.toUtf8();
        //request.toDatagram(datagram);

        socket.writeDatagram(requestDatagram.data(), requestDatagram.size(), groupAddress, 3702);

        QnSleep::msleep(150);
        while(socket.hasPendingDatagrams())
        {
            QByteArray reply;
            reply.resize(socket.pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;

            socket.readDatagram(reply.data(), reply.size(),	&sender, &senderPort);

        }

        //multicastLeaveGroup(recvSocket, groupAddress);
    }


    return result;
}
