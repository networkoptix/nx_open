// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_processor.h>

class NX_VMS_CLIENT_CORE_API QnClientResourceProcessor:
    public QObject,
    public QnResourceProcessor
{
    Q_OBJECT

public:
    virtual void processResources(const QnResourceList& resources) override;
};
