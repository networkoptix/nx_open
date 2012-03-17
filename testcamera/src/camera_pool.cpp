#include "camera_pool.h"
#include "test_camera_processor.h"
#include "plugins/resources/test_camera/testcamera_const.h"

//#include "plugins/resources/test_camera/testcamera_resource_searcher.h"

int MEDIA_PORT = 4985;

// ---------------- QnCameraDiscovery -----------------

class QnCameraDiscoveryListener: public CLLongRunnable
{
protected:
    virtual void run() override
    {
        UDPSocket m_discoverySock(TestCamConst::DISCOVERY_PORT);
        m_discoverySock.setReadTimeOut(100);
        quint8 buffer[1024*8];
        QString peerAddress;
        unsigned short peerPort;
        while(!m_needStop)
        {
            int readed = m_discoverySock.recvFrom(buffer, sizeof(buffer), peerAddress, peerPort);
            if (readed > 0)
            {
                if (QByteArray((const char*)buffer, readed).startsWith(TestCamConst::TEST_CAMERA_FIND_MSG))
                {
                    // got discovery message
                    QByteArray camResponse = QnCameraPool::instance()->getDiscoveryResponse();
                    QByteArray rez(TestCamConst::TEST_CAMERA_ID_MSG);
                    rez.append(';');
                    rez.append(camResponse);
                    m_discoverySock.setDestAddr(peerAddress, peerPort);
                    m_discoverySock.sendTo(rez.data(), rez.size());
                }
            }
        }
    }
};


// ------- QnCameraPool -----------------

Q_GLOBAL_STATIC(QnCameraPool, inst);

QnCameraPool* QnCameraPool::instance()
{
    return inst();
}

QnCameraPool::QnCameraPool():
    QnTcpListener(QHostAddress(), MEDIA_PORT),
    m_cameraNum(0)
{
    m_discoveryListener = new QnCameraDiscoveryListener();
    m_discoveryListener->start();
}   

QnCameraPool::~QnCameraPool()
{
    delete m_discoveryListener;
}

void QnCameraPool::addCameras(int count, QStringList fileList, double fps, int offlineFreq)
{
    qDebug() << "Add" << count << "cameras from file(s)" << fileList << "fps=" << fps << "offlineFreq=" << offlineFreq;
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < count; ++i)
    {
        QnTestCamera* camera = new QnTestCamera(++m_cameraNum);
        camera->setFileList(fileList);
        camera->setFps(fps);
        camera->setOfflineFreq(offlineFreq);
        m_cameras.insert(camera->getMac(), camera);
    }
}

QnTCPConnectionProcessor* QnCameraPool::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    QMutexLocker lock(&m_mutex);
    return new QnTestCameraProcessor(clientSocket, owner);
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
