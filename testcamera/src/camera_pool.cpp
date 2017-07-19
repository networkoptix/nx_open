
#include "camera_pool.h"

#include <memory>

#include <plugins/resource/test_camera/testcamera_const.h>
#include "test_camera_processor.h"

int MEDIA_PORT = 4985;

// ---------------- QnCameraDiscovery -----------------

//TODO/IMPL support multiple interface listening

class QnCameraDiscoveryListener: public QnLongRunnable
{
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
        if( m_localInterfacesToListen.isEmpty() )
            discoverySock->bind( SocketAddress( HostAddress::anyHost, TestCamConst::DISCOVERY_PORT ) );
        else
            discoverySock->bind( SocketAddress( m_localInterfacesToListen[0], TestCamConst::DISCOVERY_PORT ) );
        discoverySock->setRecvTimeout(100);
        quint8 buffer[1024*8];
        SocketAddress peerEndpoint;
        while(!m_needStop)
        {
            int readed = discoverySock->recvFrom(buffer, sizeof(buffer), &peerEndpoint);
            if (readed > 0)
            {
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

QnCameraPool::QnCameraPool(const QStringList& localInterfacesToListen,
    QnCommonModule* commonModule)
    :
    QnTcpListener(commonModule,
        localInterfacesToListen.isEmpty()
        ? QHostAddress::Any
        : QHostAddress(localInterfacesToListen[0]), MEDIA_PORT),
    m_cameraNum(0)
{
    m_discoveryListener = new QnCameraDiscoveryListener(localInterfacesToListen);
    m_discoveryListener->start();
}

QnCameraPool::~QnCameraPool()
{
    delete m_discoveryListener;
}

void QnCameraPool::addCameras(bool cameraForEachFile, int count, QStringList primaryFileList, QStringList secondaryFileList, int offlineFreq)
{
    qDebug() << "Add" << count << "cameras from primary file(s)" << primaryFileList << " secondary file(s)" << secondaryFileList << "offlineFreq=" << offlineFreq;
    QMutexLocker lock(&m_mutex);
    if (!cameraForEachFile)
    {
        for (int i = 0; i < count; ++i)
        {
            QnTestCamera* camera = new QnTestCamera(++m_cameraNum);
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

            QnTestCamera* camera = new QnTestCamera(++m_cameraNum);
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
    return new QnTestCameraProcessor(clientSocket, this);
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
