#include "coldstore_dts_reader_factory.h"

#ifdef ENABLE_COLDSTORE

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QElapsedTimer>

#include "coldstore_dts_resource_searcher.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "../../coldstore/coldstore_api/ISFS.h"
#include "utils/common/log.h"
#include "../../coldstore/coldstore_api/sfs-client.h"
#include "utils/network/socket.h"
#include "utils/network/system_socket.h"

const int coldStoreSendPort = 48102;
const int coldStoreRecvPort = 48103;

QString macFromString(QString str)
{
    str = str.toUpper();

    
    QByteArray ba = str.toLatin1();
    const char* cstr = ba.data();

    int count = 0;
    int div = 0;

    QString result;

    while(*cstr)
    {
        if ((*cstr < '0' || *cstr > '9') && (*cstr < 'A' || *cstr > 'F'))
        {
            ++cstr;
            continue;
        }

        result += QLatin1Char(*cstr);

        ++count;
        ++div;

        if (div == 2 && count < 12)
        {
            result += QLatin1String("-");
            div = 0;
        }

        if (count==12)
            break;

        ++cstr;
    }

    return result;
}
/**/

QnColdStoreDTSSearcher::QnColdStoreDTSSearcher()
{

    quint8 databuf[100];


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


    m_request = new QByteArray((const char*) databuf, sizeof(databuf));

}

QnColdStoreDTSSearcher::~QnColdStoreDTSSearcher()
{
    delete m_request;
};

QnColdStoreDTSSearcher& QnColdStoreDTSSearcher::instance()
{
    static QnColdStoreDTSSearcher inst;
    return inst;
}


QList<QnDtsUnit> QnColdStoreDTSSearcher::findDtsUnits()
{
    QList<QnDtsUnit> result;



    QList<QHostAddress> localAddresses = allLocalAddresses();

    for(const QHostAddress& localAddress: localAddresses)
    {
        QHostAddress groupAddress(QLatin1String("239.200.200.201"));

        UDPSocket sendSocket, recvSocket;

        bool bindSucceeded = sendSocket.bind(SocketAddress(localAddress.toString(), 0));

        if (!bindSucceeded)
            continue;

        recvSocket.setReuseAddrFlag(true);
        bindSucceeded = recvSocket.bind( SocketAddress(HostAddress::anyHost, coldStoreRecvPort) );
        if (!bindSucceeded)
            continue;

        if (!recvSocket.joinGroup(groupAddress.toString(), localAddress.toString()))
            continue;


        sendSocket.sendTo(m_request->data(), m_request->size(), SocketAddress(groupAddress.toString(), coldStoreSendPort));


        QElapsedTimer time;
        time.start();

        QList<QHostAddress> server_list;

        while(time.elapsed() < 500)
        {
            while (recvSocket.hasData())
            {
                QByteArray responseData;
                responseData.resize(AbstractDatagramSocket::MAX_DATAGRAM_SIZE);


                QString sender;
                quint16 senderPort;

                int readed = recvSocket.recvFrom(responseData.data(), responseData.size(), sender, senderPort);
                if (readed < 157)
                    continue;
                responseData = responseData.left(readed);

                QString csMac = QLatin1String(responseData.data() + 24);
                csMac = csMac.left(17); // mac 

                QString csName  = QLatin1String(responseData.data() + 86);

                if (csName.isEmpty())
                    continue;
                

                server_list.push_back(QHostAddress(sender));

                
            }
            QnSleep::msleep(10); // to avoid 100% cpu usage

        }

        for(const QHostAddress& srv: server_list)
        {
//			NX_LOG(QLatin1String("Found CS = "), srv.name, cl_logALWAYS);

            if (!m_factoryList.contains(srv.toString()))
                m_factoryList[srv.toString()] =  new QnColdstoreDTSreaderFactory(srv.toString());


            requestFileList(result, srv);
        }


        recvSocket.leaveGroup(groupAddress.toString());

    }

    return result;

}

void QnColdStoreDTSSearcher::requestFileList(QList<QnDtsUnit>& result, QHostAddress addr)
{
    Veracity::ISFS* sfs_client = new Veracity::SFSClient();

    QByteArray ipba = addr.toString().toLatin1();
    const char* ip = ipba.data();
    
//	NX_LOG(QLatin1String("CS checking for files"), cl_logALWAYS);

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
        "TRINITY%", // every file
        0, // channel
        false, //use_channel
        0, //limit
        false, //order_descending
        0, //time_start
        resultBA.data(),
    &return_results_sizeI, &return_results_countI);
    Q_UNUSED(status)

//	NX_LOG(QLatin1String("CS checking for files, returned: "), (int)return_results_countI, cl_logALWAYS);

    char *data = resultBA.data();
    const char* endpoint = data + return_results_sizeI;
    QString tmp;

    QStringList fileList;// = lst.split(QChar(0), QString::SkipEmptyParts);
    while (data < endpoint )
    {
        if (*data == 0) break;
        tmp = QLatin1String(data);
        data += tmp.length()+1;
        tmp.remove(0,7);
        fileList.push_back(tmp);
    }
    
    for(const QString& fn: fileList)
    {
        QString mac = fn;
        mac = mac.toUpper();
        mac = macFromString(mac);

        QnDtsUnit unit;
        unit.factory = m_factoryList[addr.toString()]; // it does exist
        unit.resourceID = mac;
                
        result.push_back(unit);
    }

    
    sfs_client->Disconnect();
    delete sfs_client;
    
}

#endif // ENABLE_COLDSTORE
