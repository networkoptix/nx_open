#ifndef av_device_server_h_2107
#define av_device_server_h_2107

#ifdef ENABLE_ARECONT

#include "core/resource_managment/resource_searcher.h"

class QnPlArecontResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlArecontResourceSearcher();

public:
    static QnPlArecontResourceSearcher& instance();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // returns all available devices
    virtual QnResourceList findResources();

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

};

#endif

#endif // av_device_server_h_2107
