
#include "camera_pool.h"

#include <memory>

#include <nx/network/nettools.h>

#include <core/resource/test_camera/testcamera_const.h>
#include "test_camera_processor.h"

int MEDIA_PORT = 4985;

// ---------------- QnCameraDiscovery -----------------

//TODO/IMPL support multiple interface listening

class QnCameraDiscoveryListener: public QnLongRunnable
{
    struct IpRangeV4
    {
        bool contains(quint32 addr) const
        {
            return qBetween(firstIp, addr, lastIp);
        }

        quint32 firstIp = 0;
        quint32 lastIp = 0;
    };

public:
    QnCameraDiscoveryListener( const QStringList& localInterfacesToListen )
    :
        m_localInterfacesToListen( localInterfacesToListen )
    {
    }

protected:
    virtual void run() override
    {
        std::unique_ptr<AbstractDatagramSocket> discoverySock( SocketFactory::createDatagramSocket().release() );
#if 1
        discoverySock->bind(SocketAddress(HostAddress::anyHost, TestCamConst::DISCOVERY_PORT));
        for (const auto& addr: m_localInterfacesToListen)
        {
            for (const QnInterfaceAndAddr& iface: getAllIPv4Interfaces())
            {
                if (iface.address == QHostAddress(addr))
                {
                    IpRangeV4 netRange;
                    netRange.firstIp = iface.subNetworkAddress().toIPv4Address();
                    netRange.lastIp = iface.broadcastAddress().toIPv4Address();
                    m_allowedIpRanges.push_back(netRange);
                }
            }
        }
#else
        // Linux doesn't support binding to specified interface and receiving broadcast packets at the same time.
        // Another option is to use SO_BINDTODEVICE socket option but it requires root access.
        if( m_localInterfacesToListen.isEmpty() )
            discoverySock->bind( SocketAddress( HostAddress::anyHost, TestCamConst::DISCOVERY_PORT ) );
        else
            discoverySock->bind( SocketAddress( m_localInterfacesToListen[0], TestCamConst::DISCOVERY_PORT ) );
#endif
        discoverySock->setRecvTimeout(100);
        quint8 buffer[1024*8];
        SocketAddress peerEndpoint;

        auto hasRange = [&](const SocketAddress& peerEndpoint)
        {
            const auto addr = peerEndpoint.address.ipV4();
            if (!addr)
                return false;
            const auto addrPtr = (quint32*)&addr->s_addr;
            const quint32 v4Addr = ntohl(*addrPtr);

            return std::any_of(
                m_allowedIpRanges.begin(),
                m_allowedIpRanges.end(),
                [v4Addr](const IpRangeV4& range)
                {
                    return range.contains(v4Addr);
                });
        };

        while(!m_needStop)
        {
            int readed = discoverySock->recvFrom(buffer, sizeof(buffer), &peerEndpoint);
            if (readed > 0)
            {
                if (!m_allowedIpRanges.isEmpty() && !hasRange(peerEndpoint))
                    continue;

                if (QByteArray((const char*)buffer, readed).startsWith(TestCamConst::TEST_CAMERA_FIND_MSG))
                {
                    // got discovery message
                    qDebug() << "Got discovery message from " << peerEndpoint.toString();
                    QByteArray camResponse = QnCameraPool::instance()->getDiscoveryResponse();
                    QByteArray rez(TestCamConst::TEST_CAMERA_ID_MSG);
                    rez.append(';');
                    rez.append(camResponse);
                    discoverySock->setDestAddr( peerEndpoint );
                    discoverySock->send(rez.data(), rez.size());
                }
            }
        }
    }

private:
    const QStringList m_localInterfacesToListen;
    QVector<IpRangeV4> m_allowedIpRanges;
};


// ------- QnCameraPool -----------------

static QnCameraPool* inst = NULL;

void QnCameraPool::initGlobalInstance( QnCameraPool* _inst )
{
    inst = _inst;
}

QnCameraPool* QnCameraPool::instance()
{
    return inst;
}

QnCameraPool::QnCameraPool(
    const QStringList& localInterfacesToListen,
    QnCommonModule* commonModule,
    bool noSecondaryStream,
    int fps)
    :
    QnTcpListener(commonModule,
        localInterfacesToListen.isEmpty()
        ? QHostAddress::Any
        : QHostAddress(localInterfacesToListen[0]), MEDIA_PORT),
    m_cameraNum(0),
    m_noSecondaryStream(noSecondaryStream),
    m_fps(fps)

{
    m_discoveryListener = new QnCameraDiscoveryListener(localInterfacesToListen);
    m_discoveryListener->start();
}

QnCameraPool::~QnCameraPool()
{
    delete m_discoveryListener;
}

void QnCameraPool::addCameras(
    bool cameraForEachFile,
    bool includePts,
    int count,
    QStringList primaryFileList,
    QStringList secondaryFileList,
    int offlineFreq)
{
    qDebug() << "Add" << count << "cameras from primary file(s)" << primaryFileList << " secondary file(s)" << secondaryFileList << "offlineFreq=" << offlineFreq;
    QMutexLocker lock(&m_mutex);
    if (!cameraForEachFile)
    {
        for (int i = 0; i < count; ++i)
        {
            QnTestCamera* camera = new QnTestCamera(++m_cameraNum, includePts);
            camera->setPrimaryFileList(primaryFileList);
            camera->setSecondaryFileList(secondaryFileList);
            camera->setOfflineFreq(offlineFreq);
            foreach(QString fileName, primaryFileList)
                QnFileCache::instance()->getMediaData(fileName);
            foreach(QString fileName, secondaryFileList)
                QnFileCache::instance()->getMediaData(fileName);
            m_cameras.insert(camera->getMac(), camera);
        }
    }
    else
    { // Special case - we run one camera for each source
        for (int i = 0; i < primaryFileList.length(); i++)
        {
            QString primaryFile = primaryFileList[i];
            QString secondaryFile = i < secondaryFileList.length() ? secondaryFileList[i] : ""; // secondary file is optional

            QnTestCamera* camera = new QnTestCamera(++m_cameraNum, includePts);
            camera->setPrimaryFileList(QStringList() << primaryFile);
            camera->setSecondaryFileList(QStringList() << secondaryFile);
            camera->setOfflineFreq(offlineFreq);

            QnFileCache::instance()->getMediaData(primaryFile);
            if (!secondaryFile.isEmpty()) // secondary file is optional
            {
                QnFileCache::instance()->getMediaData(secondaryFile);
            }
            m_cameras.insert(camera->getMac(), camera);
        }
    }
}

QnTCPConnectionProcessor* QnCameraPool::createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket)
{
    QMutexLocker lock(&m_mutex);
    return new QnTestCameraProcessor(clientSocket, this, m_noSecondaryStream, m_fps);
}

QByteArray QnCameraPool::getDiscoveryResponse()
{
    QMutexLocker lock(&m_mutex);
    QByteArray result;

    result.append(QByteArray::number(MEDIA_PORT));

    //foreach(const QString& mac, m_cameras.keys())
    for (QMap<QString, QnTestCamera*>::iterator itr = m_cameras.begin(); itr != m_cameras.end(); ++itr)
    {
        if (itr.value()->isEnabled()) {
            result.append(';');
            result.append(itr.key());
        }
    }
    return result;
}

QnTestCamera* QnCameraPool::findCamera(const QString& mac) const
{
    QMutexLocker lock(&m_mutex);
    return m_cameras.value(mac);
}
