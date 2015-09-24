#ifndef QN_SERVER_CAMERA_FACTORY_H
#define QN_SERVER_CAMERA_FACTORY_H

#include <QtCore/QObject>

#include <utils/common/singleton.h>

#include <core/resource/resource_factory.h>

class QnServerCameraFactory: public QObject, public QnResourceFactory, public Singleton<QnServerCameraFactory> {
    Q_OBJECT
public:
    QnServerCameraFactory(QObject *parent = NULL): QObject(parent) {}

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) override;
};

#endif // QN_SERVER_CAMERA_FACTORY_H
