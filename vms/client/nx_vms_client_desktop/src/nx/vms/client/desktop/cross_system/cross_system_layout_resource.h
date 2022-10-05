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
    /** Make sure `clone` method will create instance of the same class. */
    virtual LayoutResourcePtr createClonedInstance() const override;
};

using CrossSystemLayoutResourcePtr = QnSharedResourcePointer<CrossSystemLayoutResource>;
using CrossSystemLayoutResourceList = QnSharedResourcePointerList<CrossSystemLayoutResource>;

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::CrossSystemLayoutResourcePtr);
Q_DECLARE_METATYPE(nx::vms::client::desktop::CrossSystemLayoutResourceList);
