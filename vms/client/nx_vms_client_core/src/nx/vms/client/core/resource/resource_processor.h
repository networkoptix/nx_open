// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_processor.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ResourceProcessor:
    public QObject,
    public QnResourceProcessor,
    public common::SystemContextAware
{
    Q_OBJECT

public:
    ResourceProcessor(common::SystemContext* systemContext);

    virtual void processResources(const QnResourceList& resources) override;
};

} // namespace nx::vms::client::core
