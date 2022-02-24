// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>

#include <core/resource/resource_factory.h>

// TODO: #rename file
class QnClientResourceFactory: public QObject, public QnResourceFactory, public Singleton<QnClientResourceFactory> {
    Q_OBJECT
public:
    QnClientResourceFactory(QObject *parent = nullptr): QObject(parent) {}

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) override;
};
