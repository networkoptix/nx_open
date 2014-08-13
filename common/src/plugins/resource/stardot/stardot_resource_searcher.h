#ifndef _STARDOT_RESOURCE_SEARCHER_H__
#define _STARDOT_RESOURCE_SEARCHER_H__

#ifdef ENABLE_STARDOT

#include "core/resource_management/resource_searcher.h"


class QnStardotResourceSearcher : public QnAbstractNetworkResourceSearcher
{

public:
    QnStardotResourceSearcher();

    virtual QnResourcePtr createResource(const QUuid &resourceTypeId, const QnResourceParams& params) override;

    // returns all available devices
    virtual QnResourceList findResources();

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

};

#endif // #ifdef ENABLE_STARDOT
#endif // _STARDOT_RESOURCE_SEARCHER_H__
