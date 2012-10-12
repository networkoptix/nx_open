#ifndef dlink_device_server_h_2219
#define dlink_device_server_h_2219

#include "core/resource_managment/resource_searcher.h"


class QnPlDlinkResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlDlinkResourceSearcher();

public:
    static QnPlDlinkResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QnResourcePtr checkHostAddr(const QUrl& url, const QAuthenticator& auth) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

};

#endif // dlink_device_server_h_2219
