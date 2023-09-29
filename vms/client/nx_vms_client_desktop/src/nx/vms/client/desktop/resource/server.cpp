// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server.h"

namespace nx::vms::client::desktop {

void ServerResource::setCompatible(bool value)
{
    if (m_isCompatible == value)
        return;

    m_isCompatible = value;
    emit compatibilityChanged(::toSharedPointer(this));
}

bool ServerResource::isCompatible() const
{
    return m_isCompatible;
}

void ServerResource::setDetached(bool value)
{
    if (m_isDetached == value)
        return;

    m_isDetached = value;
    emit isDetachedChanged();
}

bool ServerResource::isDetached() const
{
    return m_isDetached;
}

} // namespace nx::vms::client::desktop
