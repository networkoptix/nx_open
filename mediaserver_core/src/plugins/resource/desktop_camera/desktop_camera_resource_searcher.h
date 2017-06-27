#ifndef _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
#define _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__

#ifdef ENABLE_DESKTOP_CAMERA

#include <QtCore/QElapsedTimer>

#include "plugins/resource/upnp/upnp_resource_searcher.h"
#include <nx/network/simple_http_client.h>

#include <nx/utils/singleton.h>


class QnDesktopCameraResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public Singleton<QnDesktopCameraResourceSearcher>
{
    typedef QnAbstractNetworkResourceSearcher base_type;
public:
    QnDesktopCameraResourceSearcher(QnCommonModule* commonModule);
    virtual ~QnDesktopCameraResourceSearcher();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

    virtual QnResourceList findResources(void) override;

    void registerCamera(const QSharedPointer<AbstractStreamSocket>& connection,
		                const QString& userName, const QString &userId);


    quint32 incCSeq(const TCPSocketPtr& socket);

    TCPSocketPtr acquireConnection(const QString& userId);
    void releaseConnection(const TCPSocketPtr& socket);

    virtual bool isVirtualResource() const override { return true; }

    /** Check if camera really connected to our server. */
    bool isCameraConnected(const QnVirtualCameraResourcePtr &camera);

private:
    void cleanupConnections();

    /**
     * Check if this uniqueId is present in the connections list.
     * Does NOT lock the mutex.
     */
    bool isClientConnectedInternal(const QString &uniqueId) const;
private:
    struct ClientConnectionInfo
    {
        ClientConnectionInfo(const TCPSocketPtr& socket, const QString &userName, const QString &userId):
            socket(socket),
            userName(userName),
            userId(userId)
        {
            useCount = 0;
            cSeq = 0;
            timer.restart();
        }
        TCPSocketPtr socket;
        int useCount;
        quint32 cSeq;
        QElapsedTimer timer;
        QString userName;
        QString userId;
    };

    QList<ClientConnectionInfo> m_connections;
    QnMutex m_mutex;

private:
    QnSecurityCamResourcePtr cameraFromConnection(const ClientConnectionInfo& info);
    void log(const QByteArray &message, const ClientConnectionInfo &info) const;
};

#endif //ENABLE_DESKTOP_CAMERA

#endif // _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
