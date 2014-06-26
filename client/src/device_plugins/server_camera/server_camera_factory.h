#ifndef QN_SERVER_CAMERA_FACTORY_H
#define QN_SERVER_CAMERA_FACTORY_H

#include <core/resource/resource_factory.h>

class QnServerCameraFactory: public QnResourceFactory {
public:
    virtual QnResourcePtr createResource(const QnId &resourceTypeId, const QnResourceParams &params) override;

    static QnServerCameraFactory &instance();
};

#endif // QN_SERVER_CAMERA_FACTORY_H
