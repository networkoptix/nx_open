#ifndef __CAMERA_POOL_H__
#define __CAMERA_POOL_H__

#include <QtCore/QMap>
#include <QtCore/QMutex>
#include "test_camera.h"
#include <nx/network/socket.h>
#include <utils/common/long_runnable.h>
#include <network/tcp_listener.h>

class QnCameraDiscoveryListener;

class QnCameraPool: public QnTcpListener
{
public:
    /*!
        \param localInterfacesToListen If empty, all local interfaces are listened
    */
    QnCameraPool(const QStringList& localInterfacesToListen, QnCommonModule* commonModule);
    virtual ~QnCameraPool();

    static void initGlobalInstance(QnCameraPool* _inst);
    static QnCameraPool* instance();

    void addCameras(bool cameraForEachFile, int count, QStringList primaryFileList, QStringList secondaryFileList, int offlineFreq);
    QnTestCamera* findCamera(const QString& mac) const;
    QByteArray getDiscoveryResponse();
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket);
private:
    QMap<QString, QnTestCamera*> m_cameras;
    int m_cameraNum;
    mutable QMutex m_mutex;
    QnCameraDiscoveryListener* m_discoveryListener;
};

#endif // __CAMERA_POOL_H__
