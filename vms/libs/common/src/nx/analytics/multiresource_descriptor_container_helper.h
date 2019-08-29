#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnResourcePool;

namespace nx::analytics {

class MultiresourceDescriptorContainerHelper: public Connective<QObject>
{
    Q_OBJECT
public:
    using OnServerAddedHandler = std::function<void(QnMediaServerResourcePtr)>;
    using OnServerRemovedHandler = std::function<void(QnMediaServerResourcePtr)>;

public:
    MultiresourceDescriptorContainerHelper(
        QnResourcePool* resourcePool,
        OnServerAddedHandler onServerAddedHandler,
        OnServerRemovedHandler onServerRemovedHandler);

public slots:
    void at_resourceAdded(QnResourcePtr resource);
    void at_resourceRemoved(QnResourcePtr resource);

private:
    OnServerAddedHandler m_onServerAddedHandler;
    OnServerRemovedHandler m_onServerRemovedHandler;
};

} // namespace nx::analytics
