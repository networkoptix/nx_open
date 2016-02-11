#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>

#include <core/resource/resource_factory.h>

class QnClientCameraFactory: public QObject, public QnResourceFactory, public Singleton<QnClientCameraFactory> {
    Q_OBJECT
public:
    QnClientCameraFactory(QObject *parent = NULL): QObject(parent) {}

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) override;
};
