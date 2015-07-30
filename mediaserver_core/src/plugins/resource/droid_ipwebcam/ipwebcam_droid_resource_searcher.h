#ifndef ipwebcam_droid_device_server_h_1657
#define ipwebcam_droid_device_server_h_1657

#ifdef ENABLE_DROID

#include "core/resource_management/resource_searcher.h"

class QnPlIpWebCamResourceSearcher : public QnAbstractNetworkResourceSearcher
{

public:
    QnPlIpWebCamResourceSearcher();

    QnResourceList findResources();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
   
};

#endif // #ifdef ENABLE_DROID
#endif //ipwebcam_droid_device_server_h_1657
