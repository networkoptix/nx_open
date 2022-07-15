// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/layout_resource.h>
#include <core/resource/shared_resource_pointer.h>
#include <core/resource/shared_resource_pointer_list.h>

namespace nx::vms::api { struct LayoutData; }

namespace nx::vms::client::desktop {

class CrossSystemLayoutResource: public QnLayoutResource
{
public:
    CrossSystemLayoutResource();

    void update(const nx::vms::api::LayoutData& layoutData);
};

using CrossSystemLayoutResourcePtr = QnSharedResourcePointer<CrossSystemLayoutResource>;
using CrossSystemLayoutResourceList = QnSharedResourcePointerList<CrossSystemLayoutResource>;

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::CrossSystemLayoutResourcePtr);
Q_DECLARE_METATYPE(nx::vms::client::desktop::CrossSystemLayoutResourceList);
