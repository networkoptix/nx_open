// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>
#include <nx/vms/client/core/resource/resource_factory.h>

class QnMobileClientResourceFactory:
    public QObject,
    public nx::vms::client::core::ResourceFactory,
    public Singleton<QnMobileClientResourceFactory>
{
    Q_OBJECT
public:
    QnMobileClientResourceFactory(QObject* parent = NULL): QObject(parent) {}

    virtual QnResourcePtr createResource(
        const nx::Uuid& resourceTypeId, const QnResourceParams& params) override;
};
