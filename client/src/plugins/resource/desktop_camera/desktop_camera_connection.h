#ifndef __DESKTOP_CAMERA_CONNECTION_H__
#define __DESKTOP_CAMERA_CONNECTION_H__

#include <QSharedPointer>
#include "utils/common/long_runnable.h"
#include "core/resource/resource_fwd.h"
#include <nx/network/simple_http_client.h>
#include "network/tcp_connection_processor.h"
#include <nx/network/http/httpclient.h>

class QnDesktopResource;
class QnTCPConnectionProcessor;
class QnDesktopCameraConnectionProcessorPrivate;
class QnDesktopCameraConnectionProcessor;

/*
*   This class used for connection from desktop camera to server
*/

class QnDesktopCameraConnection: public QnLongRunnable
{
public:
    typedef QnLongRunnable base_type;

    QnDesktopCameraConnection(QnDesktopResource* owner, const QnMediaServerResourcePtr &server);
    virtual ~QnDesktopCameraConnection();

    virtual void pleaseStop() override;
protected:
    virtual void run() override;
private:
    void terminatedSleep(int sleep);
private:
    QnDesktopResource* m_owner;
    QnMediaServerResourcePtr m_server;
    std::shared_ptr<QnDesktopCameraConnectionProcessor> processor;
    QSharedPointer<AbstractStreamSocket> tcpSocket;
    std::unique_ptr<nx_http::HttpClient> httpClient;
    QnMutex m_mutex;
};

typedef QSharedPointer<QnDesktopCameraConnection> QnDesktopCameraConnectionPtr;

class QnDesktopCameraConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnDesktopCameraConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, void* sslContext, QnDesktopResource* desktop);
    virtual ~QnDesktopCameraConnectionProcessor();
    void processRequest();
    void sendData(const QnByteArray& data);
    void sendData(const char* data, int len);

    void sendUnlock();
    void sendLock();
    bool isConnected() const;
private:
    void disconnectInternal();
private:
    Q_DECLARE_PRIVATE(QnDesktopCameraConnectionProcessor);
};


#endif // __DESKTOP_CAMERA_CONNECTION_H__
