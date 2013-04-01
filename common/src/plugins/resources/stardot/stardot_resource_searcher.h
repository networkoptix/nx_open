#ifndef _STARDOT_RESOURCE_SEARCHER_H__
#define _STARDOT_RESOURCE_SEARCHER_H__

#include "core/resource_managment/resource_searcher.h"


class QnStardotResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnStardotResourceSearcher();

public:
    static QnStardotResourceSearcher& instance();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // returns all available devices
    virtual QnResourceList findResources();

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

};

#endif // _STARDOT_RESOURCE_SEARCHER_H__
