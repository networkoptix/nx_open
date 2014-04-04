#ifndef _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
#define _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__

#ifdef ENABLE_DESKTOP_CAMERA

#include <QtCore/QElapsedTimer>

#include "plugins/resources/upnp/upnp_resource_searcher.h"
#include "utils/network/simple_http_client.h"


class QnDesktopCameraResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnDesktopCameraResourceSearcher();
public:
    static QnDesktopCameraResourceSearcher& instance();
    
    virtual ~QnDesktopCameraResourceSearcher();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

    virtual QnResourceList findResources(void) override;

    void registerCamera(QSharedPointer<AbstractStreamSocket> connection, const QString& userName);

    TCPSocketPtr getConnection(const QString& userName);
    quint32 incCSeq(const TCPSocketPtr socket);
    void releaseConnection(TCPSocketPtr socket);

    virtual bool isVirtualResource() const override { return true; }
private:
    struct ClientConnectionInfo
    {
        ClientConnectionInfo(TCPSocketPtr _socket, const QString& _userName)
        {
            socket = _socket;
            useCount = 0;
            cSeq = 0;
            timer.restart();
            userName = _userName;
        }
        TCPSocketPtr socket;
        int useCount;
        quint32 cSeq;
        QElapsedTimer timer;
        QString userName;
    };

    QList<ClientConnectionInfo> m_connections;
    QMutex m_mutex;
private:
    void cleanupConnections();
};

#endif //ENABLE_DESKTOP_CAMERA

#endif // _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
