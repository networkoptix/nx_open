#ifndef __TEST_CAMERA_RESOURCE_SEARCHER_H_
#define __TEST_CAMERA_RESOURCE_SEARCHER_H_

#include "core/resourcemanagment/resource_searcher.h"


class QnTestCameraResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnTestCameraResourceSearcher();
public:
    static QnTestCameraResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

};

#endif // __TEST_CAMERA_RESOURCE_SEARCHER_H_
