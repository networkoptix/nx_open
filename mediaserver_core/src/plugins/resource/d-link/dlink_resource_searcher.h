#ifndef dlink_device_server_h_2219
#define dlink_device_server_h_2219

#ifdef ENABLE_DLINK

#include "core/resource_management/resource_searcher.h"


class QnPlDlinkResourceSearcher : public QnAbstractNetworkResourceSearcher
{

public:
    QnPlDlinkResourceSearcher(QnCommonModule* commonModule);

    QnResourceList findResources(void);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QnResourceList checkEndpoint(
        const QUrl& url, const QAuthenticator& auth,
        const QString& physicalId, QnResouceSearchMode mode) override;

protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

};

#endif // ENABLE_DLINK
#endif // dlink_device_server_h_2219
