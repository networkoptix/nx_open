#pragma once

#ifdef ENABLE_DESKTOP_CAMERA

#include <QtCore/QElapsedTimer>

#include "plugins/resource/upnp/upnp_resource_searcher.h"
#include <nx/network/deprecated/simple_http_client.h>

#include <nx/utils/singleton.h>
#include <nx/utils/url.h>


class QnDesktopCameraResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public Singleton<QnDesktopCameraResourceSearcher>
{
    typedef QnAbstractNetworkResourceSearcher base_type;
public:
    explicit QnDesktopCameraResourceSearcher(QnCommonModule* commonModule);
    virtual ~QnDesktopCameraResourceSearcher() override;

    virtual QnResourcePtr createResource(const QnUuid& resourceTypeId,
        const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

    virtual QnResourceList findResources() override;

    void registerCamera(std::unique_ptr<nx::network::AbstractStreamSocket> connection,
        const QString& userName,
        const QString& uniqueId);

    quint32 incCSeq(const QSharedPointer<nx::network::AbstractStreamSocket>& socket);

    QSharedPointer<nx::network::AbstractStreamSocket> acquireConnection(const QString& userId);
    void releaseConnection(const QSharedPointer<nx::network::AbstractStreamSocket>& socket);

    virtual bool isVirtualResource() const override { return true; }

    /** Check if camera really connected to our server. */
    bool isCameraConnected(const QnVirtualCameraResourcePtr& camera);

private:
    struct ClientConnectionInfo;

    static QnSecurityCamResourcePtr cameraFromConnection(const ClientConnectionInfo& info);
    void log(const QByteArray& message, const ClientConnectionInfo& info) const;

    void cleanupConnections();

    /**
     * Check if this uniqueId is present in the connections list.
     * Does NOT lock the mutex.
     */
    bool isClientConnectedInternal(const QString& uniqueId) const;

private:
    QList<ClientConnectionInfo> m_connections;
    QnMutex m_mutex;
};

#endif //ENABLE_DESKTOP_CAMERA
