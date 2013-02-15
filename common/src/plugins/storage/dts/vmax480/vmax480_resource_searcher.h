#ifndef vmax480_resource_searcher_h_1806
#define vmax480_resource_searcher_h_1806

#include "core/resource_managment/resource_searcher.h"

class QnPlVmax480ResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlVmax480ResourceSearcher();

public:
    static QnPlVmax480ResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

};



#endif //vmax480_resource_searcher_h_1806