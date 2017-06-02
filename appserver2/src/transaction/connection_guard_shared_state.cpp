#include "connection_guard_shared_state.h"

namespace ec2 {

    bool ConnectionGuardSharedState::contains(const QnUuid& id) const
    {
        QnMutexLocker lock(&m_mutex);
        return m_connectedList.contains(id);
    }

} // namespace ec2
