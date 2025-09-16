// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

namespace nx::analytics { class AbstractMetadataConsumer;}

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API AbstractMetadataConsumerOwner
{
public:
    virtual ~AbstractMetadataConsumerOwner() {}

    virtual QSharedPointer<nx::analytics::AbstractMetadataConsumer> metadataConsumer() const = 0;
};

} // namespace nx::vms::client::core
