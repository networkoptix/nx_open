#pragma once

#include <QtCore/QSharedPointer>

#include <client_core/connection_context_aware.h>

#include <nx/utils/thread/long_runnable.h>
#include "core/resource/resource_fwd.h"
#include <nx/network/simple_http_client.h>
#include "network/tcp_connection_processor.h"
#include <nx/network/http/http_client.h>

class QnDesktopResource;
class QnTCPConnectionProcessor;
class QnDesktopCameraConnectionProcessorPrivate;
class QnDesktopCameraConnectionProcessor;

/*
*   This class used for connection from desktop camera to server
*/

class QnDesktopCameraConnection: public QnLongRunnable, public QnConnectionContextAware
{
public:
    typedef QnLongRunnable base_type;

    QnDesktopCameraConnection(QnDesktopResource* owner,
        const QnMediaServerResourcePtr& server,
        const QnUuid& userId);
    virtual ~QnDesktopCameraConnection();

    virtual void pleaseStop() override;
protected:
    virtual void run() override;
private:
    void terminatedSleep(int sleep);
    QSharedPointer<AbstractStreamSocket> takeSocketFromHttpClient(
        std::unique_ptr<nx_http::HttpClient>& httpClient);
private:
    QnDesktopResource* m_owner;
    QnMediaServerResourcePtr m_server;
    QnUuid m_userId;
    std::shared_ptr<QnDesktopCameraConnectionProcessor> processor;
    QSharedPointer<AbstractStreamSocket> tcpSocket;
    std::unique_ptr<nx_http::HttpClient> httpClient;
    QnMutex m_mutex;
};

using QnDesktopCameraConnectionPtr = std::unique_ptr<QnDesktopCameraConnection>;

class QnDesktopCameraConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnDesktopCameraConnectionProcessor(
        QSharedPointer<AbstractStreamSocket> socket,
        void* sslContext,
        QnDesktopResource* desktop);
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
