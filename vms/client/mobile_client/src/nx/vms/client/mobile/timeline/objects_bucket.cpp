// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "objects_bucket.h"

namespace nx::vms::client::mobile {
namespace timeline {

MultiObjectData ObjectBucket::getData() const
{
    return data.value_or(MultiObjectData{});
}

} // namespace timeline
} // namespace nx::vms::client::mobile
