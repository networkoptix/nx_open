#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>

#include <core/resource/resource_factory.h>

//TODO: #rename file
class QnClientResourceFactory: public QObject, public QnResourceFactory, public Singleton<QnClientResourceFactory> {
    Q_OBJECT
public:
    QnClientResourceFactory(QObject *parent = NULL): QObject(parent) {}

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) override;
};
