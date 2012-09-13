#ifndef __TEST_CAMERA_RESOURCE_SEARCHER_H_
#define __TEST_CAMERA_RESOURCE_SEARCHER_H_

#include "core/resourcemanagment/resource_searcher.h"


class QnTestCameraResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    Q_DECLARE_TR_FUNCTIONS(QnTestCameraResourceSearcher)
    QnTestCameraResourceSearcher();
public:
    static QnTestCameraResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(const QUrl& url, const QAuthenticator& auth);

};

#endif // __TEST_CAMERA_RESOURCE_SEARCHER_H_
