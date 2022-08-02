// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/media_server_resource.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ServerResource: public QnMediaServerResource
{
    using base_type = QnMediaServerResource;

public:
    virtual void setStatus(
        nx::vms::api::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;
};

} // namespace nx::vms::client::core
