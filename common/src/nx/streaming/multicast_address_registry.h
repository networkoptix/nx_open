#pragma once

#include <map>

#include <core/resource/resource_fwd.h>
#include <nx/network/socket_common.h>
#include <common/common_module_aware.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace streaming {

class MulticastAddressRegistry: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:
    MulticastAddressRegistry(QnCommonModule* parent = nullptr);

    bool registerAddress(
        QnVirtualCameraResourcePtr resource,
        SocketAddress address);
    bool unregisterAddress(SocketAddress address);

    QnVirtualCameraResourcePtr addressUser(const SocketAddress& address) const;
private:
    mutable QnMutex m_mutex;
    std::map<SocketAddress, QWeakPointer<QnVirtualCameraResource>> m_registry;
};

} // namespace streaming
} // namespace nx
