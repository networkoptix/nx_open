// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/system_context_accessor.h>

class QnCameraThumbnailCache;

namespace nx::vms::client::mobile {

class SystemContext;

class SystemContextAccessor: public core::SystemContextAccessor
{
    Q_OBJECT
    using base_type = core::SystemContextAccessor;

public:
    using base_type::base_type;

    SystemContext* mobileSystemContext() const;

    QnCameraThumbnailCache* cameraThumbnailCache() const;
};

} // nx::vms::client::mobile
