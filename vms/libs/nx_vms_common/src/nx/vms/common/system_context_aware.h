// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <nx/utils/impl_ptr.h>

class QnCameraHistoryPool;
class QnCommonMessageProcessor;
class QnResourceAccessManager;
class QnResourcePool;

namespace nx::core::access { class ResourceAccessProvider; }

namespace nx::vms::common {

class SystemContext;
class SystemSettings;

class NX_VMS_COMMON_API SystemContextInitializer
{
public:
    virtual ~SystemContextInitializer() = default;
    virtual SystemContext* systemContext() const = 0;
};

/**
 * Helper class for the SystemContext-dependent classes. Must be destroyed before the Context is.
 */
class NX_VMS_COMMON_API SystemContextAware
{
public:
    SystemContextAware(SystemContext* context);
    SystemContextAware(std::unique_ptr<SystemContextInitializer> initializer);
    ~SystemContextAware();

    /**
     * Linked context. May never be changed. Must exist when SystemContextAware is destroyed.
     */
    SystemContext* systemContext() const;

protected:
    /**
     * Manages which permissions User has on each of its accessible Resources.
     */
    QnResourceAccessManager* resourceAccessManager() const;

    /**
     * Grants information about resource access status.
     */
    nx::core::access::ResourceAccessProvider* resourceAccessProvider() const;

    /**
     * List of all Resources in the System. Some data is stored in the external dictionaries.
     */
    QnResourcePool* resourcePool() const;

    QnCommonMessageProcessor* messageProcessor() const;

    /**
     * Information about Servers, storing Device footage.
     */
    QnCameraHistoryPool* cameraHistoryPool() const;

    /**
     * System settings, which do not depend on any Device or Server and are applied globally.
     * Currently stored as Resource Properties for the `admin` User.
     */
    SystemSettings* systemSettings() const;

    // TODO: #GDM Remove field.
    SystemContext* m_context = nullptr;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common
