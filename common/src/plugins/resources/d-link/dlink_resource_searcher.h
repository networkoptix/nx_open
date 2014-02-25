#ifndef dlink_device_server_h_2219
#define dlink_device_server_h_2219

#ifdef ENABLE_DLINK

#include "core/resource_management/resource_searcher.h"


class QnPlDlinkResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlDlinkResourceSearcher();

public:
    static QnPlDlinkResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QString& url);

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

};

#endif // ENABLE_DLINK
#endif // dlink_device_server_h_2219
