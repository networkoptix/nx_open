// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource/layout_resource.h>

namespace nx::vms::client::desktop {

struct CrossSystemLayoutData;

class CrossSystemLayoutResource: public LayoutResource
{
public:
    CrossSystemLayoutResource();

    void update(const CrossSystemLayoutData& layoutData);

protected:
    /** Virtual constructor for cloning. */
    virtual LayoutResourcePtr create() const override;
};

using CrossSystemLayoutResourcePtr = QnSharedResourcePointer<CrossSystemLayoutResource>;
using CrossSystemLayoutResourceList = QnSharedResourcePointerList<CrossSystemLayoutResource>;

} // namespace nx::vms::client::desktop
