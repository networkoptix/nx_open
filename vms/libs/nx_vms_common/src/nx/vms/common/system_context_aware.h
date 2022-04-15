// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <nx/utils/impl_ptr.h>

class QnResourceAccessManager;
class QnResourcePool;

namespace nx::vms::common {

class SystemContext;

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
     * List of all Resources in the System. Some data is stored in the external dictionaries.
     */
    QnResourcePool* resourcePool() const;

    // TODO: #GDM Remove field.
    SystemContext* m_context = nullptr;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common
