#ifndef _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
#define _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__

#include "plugins/resources/upnp/upnp_resource_searcher.h"
#include "utils/network/simple_http_client.h"


class QnDesktopCameraResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnDesktopCameraResourceSearcher();
public:
    static QnDesktopCameraResourceSearcher& instance();
    
    virtual ~QnDesktopCameraResourceSearcher();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

    virtual QnResourceList findResources(void) override;

    void registerCamera(TCPSocket* connection, const QString& userName);

    TCPSocketPtr getConnection(const QString& userName);
private:
    QMap<QString, TCPSocketPtr> m_connections;
    void cleanupConnections();
};

#endif // _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
