// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_guard_shared_state.h"

namespace ec2 {

    bool ConnectionGuardSharedState::contains(const nx::Uuid& id) const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_connectedList.contains(id);
    }

} // namespace ec2
