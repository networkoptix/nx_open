// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../abstract_desktop_resource_searcher_impl.h"

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API DesktopAudioOnlyResourceSearcherImpl:
    public QnAbstractDesktopResourceSearcherImpl
{
    virtual QnResourceList findResources() override;
};

} // namespace nx::vms::client::core
