#include "coldstore_device_server.h"
#include "network/nettools.h"
#include "coldstore_api/sfs-client.h"
#include "coldstore_device.h"

const int coldStoreSendPort = 48102;
const int coldStoreRecvPort = 48103;

extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);


extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);



ColdStoreDeviceServer::ColdStoreDeviceServer()
{

    char databuf[100];


    databuf[0] = 0x03;
    databuf[1] = 0x10;
    databuf[2] = 0x20;
    databuf[3] = 0x00;
    for (int i = 4; i < 88; i++)
        databuf[i] = 0x00;
    databuf[88] = 0x41;
    databuf[89] = 0x7e;
    databuf[90] = 0x47;
    databuf[91] = 0xf6;


    m_request = new QByteArray(databuf, sizeof(databuf));

}

ColdStoreDeviceServer::~ColdStoreDeviceServer()
{
    delete m_request;
};

ColdStoreDeviceServer& ColdStoreDeviceServer::instance()
{
    static ColdStoreDeviceServer inst;
    return inst;
}

bool ColdStoreDeviceServer::isProxy() const
{
    return false;
}

QString ColdStoreDeviceServer::name() const
{
    return QLatin1String("ColdStore");
}



CLDeviceList ColdStoreDeviceServer::findDevices()
{
    CLDeviceList result;



    QList<QHostAddress> localAddresses = getAllIPv4Addresses();

    foreach(QHostAddress localAddress, localAddresses)
    {
        QHostAddress groupAddress(QLatin1String("239.200.200.201"));

        QUdpSocket sendSocket, recvSocket;

        bool bindSucceeded = sendSocket.bind(localAddress, 0);

        if (!bindSucceeded)
            return result;

        bindSucceeded = recvSocket.bind(QHostAddress::Any, coldStoreRecvPort, QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);
        if (!bindSucceeded)
            return result;

        if (!multicastJoinGroup(recvSocket, groupAddress, localAddress))      
            continue;


        sendSocket.writeDatagram(m_request->data(), m_request->size(), groupAddress, coldStoreSendPort);


        QTime time;
        time.start();

        while(time.elapsed() < 500)
        {
            while (recvSocket.hasPendingDatagrams())
            {
                QByteArray responseData;
                responseData.resize(recvSocket.pendingDatagramSize());

                if (recvSocket.pendingDatagramSize() < 157)
                    continue;

                QHostAddress sender;
                quint16 senderPort;

                recvSocket.readDatagram(responseData.data(), responseData.size(),	&sender, &senderPort);

                QString csMac = QString(responseData.data() + 24);
                csMac = csMac.left(17); // mac 

                QString csName  = QString(responseData.data() + 86);
                csName = csName;

                QString uNmae = csName + QString(" ") + csMac;

                requestFileList(result, csMac, csName, sender);

                
            }
        }



        multicastLeaveGroup(recvSocket, groupAddress);

    }

    return result;

}

void ColdStoreDeviceServer::requestFileList(CLDeviceList& result, const QString& csid, const QString& csName,  QHostAddress addr)
{
    Veracity::ISFS* sfs_client = new Veracity::SFSClient();

    QByteArray ipba = addr.toString().toLocal8Bit();
    const char* ip = ipba.data();

    if (sfs_client->Connect(ip) != Veracity::ISFS::STATUS_SUCCESS)
    {
        delete sfs_client;
        return;
    }

    

    Veracity::u32 return_results_sizeI;
    Veracity::u32 return_results_countI;
    QByteArray resultBA(1024*1024,  0);
    

    Veracity::u32 status = sfs_client->FileQuery(
        true,  // distinct; true - do not dublicate resilts
        false, // false ( channel )
        true, // filename
        false, //return_first_ms
        0, // string_start
        0, // string_length
        "%", // every file
        0, // channel
        false, //use_channel
        0, //limit
        false, //order_descending
        0, //time_start
        resultBA.data(),
    &return_results_sizeI, &return_results_countI);

    QString lst(resultBA);

    QStringList fileList = lst.split(QChar(0), QString::SkipEmptyParts);
    
    foreach(const QString& fn, fileList)
    {
        QString id = csid + fn;

        if (!result.contains(id))
        {
            ColdStoreDevice* dev = new ColdStoreDevice(id,  csName + QString(" ") + fn, fn, addr);
            result[dev->getUniqueId()] = dev;
        }
    }

    
    sfs_client->Disconnect();
    delete sfs_client;
    
}