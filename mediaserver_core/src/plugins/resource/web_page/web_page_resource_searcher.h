#pragma once

#include <core/resource_management/resource_searcher.h>

struct QnResourceParams;

class QnWebPageResourceSearcher: public QnAbstractNetworkResourceSearcher {
public:
    QnWebPageResourceSearcher();

    //!Implementation of QnResourceFactory::createResource
    virtual QnResourcePtr createResource( const QnUuid &resourceTypeId, const QnResourceParams &params ) override;

    // return the manufacture of the resource
    // !Implementation of QnResourceFactory::manufacture
    virtual QString manufacture() const override;

    //!Implementation of QnAbstractNetworkResourceSearcher::checkHostAddr
    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

    //!Implementation of QnAbstractResourceSearcher::findResources
    virtual QnResourceList findResources() override;

    virtual bool isResourceTypeSupported(QnUuid resourceTypeId) const;
};