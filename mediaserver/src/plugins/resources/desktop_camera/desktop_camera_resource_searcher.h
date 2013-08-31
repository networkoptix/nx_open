#ifndef _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
#define _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QElapsedTimer>
#include "plugins/resources/upnp/upnp_resource_searcher.h"


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
};

#endif // _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
