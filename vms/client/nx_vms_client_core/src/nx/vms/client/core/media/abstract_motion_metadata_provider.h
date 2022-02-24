// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <chrono>

#include <QtCore/QScopedPointer>

struct QnMetaDataV1;

namespace nx::vms::client::core {

typedef std::shared_ptr<QnMetaDataV1> MetaDataV1Ptr;

class AbstractMotionMetadataProvider
{
public:
    AbstractMotionMetadataProvider();
    virtual ~AbstractMotionMetadataProvider();

    virtual MetaDataV1Ptr metadata(
        const std::chrono::microseconds timestamp, int channel) const = 0;
};

} // namespace nx::vms::client::core
