#pragma once

#include <QtCore/QObject>

#include <utils/common/singleton.h>

#include <core/resource/resource_factory.h>

class QnMobileClientCameraFactory: public QObject, public QnResourceFactory, public Singleton<QnMobileClientCameraFactory> {
    Q_OBJECT
public:
    QnMobileClientCameraFactory(QObject *parent = NULL): QObject(parent) {}

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) override;
};
