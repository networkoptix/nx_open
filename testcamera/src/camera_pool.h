#ifndef __CAMERA_POOL_H__
#define __CAMERA_POOL_H__

#include <QMap>
#include "test_camera.h"
#include "utils/network/socket.h"
#include "utils/common/longrunnable.h"
#include "utils/network/tcp_listener.h"

class QnCameraDiscoveryListener;

class QnCameraPool: public QnTcpListener
{
public:
    QnCameraPool();
    virtual ~QnCameraPool();
    static QnCameraPool* instance();
    
    void addCameras(int count, QStringList fileList, double fps, int offlineFreq);
    QnTestCamera* findCamera(const QString& mac) const;
    QByteArray getDiscoveryResponse();
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner);
private:
    QMap<QString, QnTestCamera*> m_cameras;
    int m_cameraNum;
    mutable QMutex m_mutex;
    QnCameraDiscoveryListener* m_discoveryListener;
};

#endif // __CAMERA_POOL_H__
