#ifndef _ACTI_RESOURCE_SEARCHER_H__
#define _ACTI_RESOURCE_SEARCHER_H__

#include "core/resource_managment/resource_searcher.h"
#include "../mdns/mdns_device_searcher.h"


class QnActiResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnActiResourceSearcher();

public:
    static QnActiResourceSearcher& instance();
    
    virtual ~QnActiResourceSearcher();

    virtual QnResourceList findResources(void) override { return QnResourceList(); }
    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
private:
    QMap<QString, QUdpSocket*> m_socketList;
};

#endif // _ACTI_RESOURCE_SEARCHER_H__
