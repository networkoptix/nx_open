#ifndef ipwebcam_droid_device_server_h_1657
#define ipwebcam_droid_device_server_h_1657

#include "core/resource_managment/resource_searcher.h"

class QnPlIpWebCamResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlIpWebCamResourceSearcher();

public:
    static QnPlIpWebCamResourceSearcher& instance();

    QnResourceList findResources();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
   
};

#endif //ipwebcam_droid_device_server_h_1657
