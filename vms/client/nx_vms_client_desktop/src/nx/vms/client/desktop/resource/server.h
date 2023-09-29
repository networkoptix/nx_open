// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/server.h>

#include "resource_fwd.h"

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API ServerResource: public nx::vms::client::core::ServerResource
{
    Q_OBJECT
    using base_type = nx::vms::client::core::ServerResource;

public:
    void setCompatible(bool value);
    bool isCompatible() const;

    void setDetached(bool value);
    bool isDetached() const;

signals:
    void compatibilityChanged(const QnResourcePtr& resource);
    void isDetachedChanged();

private:
    bool m_isCompatible = true;
    bool m_isDetached = false;
};

using ServerResourcePtr = QnSharedResourcePointer<ServerResource>;

} // namespace nx::vms::client::desktop
