#ifndef __DESKTOP_CAMERA_CONNECTION_H__
#define __DESKTOP_CAMERA_CONNECTION_H__

#include <QSharedPointer>
#include "utils/common/long_runnable.h"
#include "core/resource/resource_fwd.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/tcp_connection_processor.h"

class QnDesktopResource;
class QnTCPConnectionProcessor;
class QnDesktopCameraConnectionProcessorPrivate;

/*
*   This class used for connection from desktop camera to media server
*/

class QnDesktopCameraConnection: public QnLongRunnable
{
public:
    QnDesktopCameraConnection(QnDesktopResource* owner, QnMediaServerResourcePtr mServer);
    virtual ~QnDesktopCameraConnection();
protected:
    virtual void run() override;
private:
    void terminatedSleep(int sleep);
private:
    QnDesktopResource* m_owner;
    QnMediaServerResourcePtr m_mServer;
};

typedef QSharedPointer<QnDesktopCameraConnection> QnDesktopCameraConnectionPtr;

class QnDesktopCameraConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnDesktopCameraConnectionProcessor(TCPSocket* socket, void* sslContext, QnDesktopResource* desktop);
    void processRequest();
    void sendData(const QnByteArray& data);
private:
    Q_DECLARE_PRIVATE(QnDesktopCameraConnectionProcessor);
};


#endif // __DESKTOP_CAMERA_CONNECTION_H__
