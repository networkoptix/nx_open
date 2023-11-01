// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <nx/utils/impl_ptr.h>

class QnCameraHistoryPool;
class QnCommonMessageProcessor;
class QnGlobalPermissionsManager;
class QnResourceAccessManager;
class QnResourcePool;
class QnRuntimeInfoManager;

namespace ec2 { class AbstractECConnection; }

namespace nx::vms::common {

class LookupListManager;
class ShowreelManager;
class SystemContext;
class SystemSettings;
class UserGroupManager;

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
    virtual ~SystemContextAware();

    /**
     * Linked context. May never be changed. Must exist when SystemContextAware is destroyed.
     */
    SystemContext* systemContext() const;

    /**
     * Interface for the Message Bus connection.
     */
    std::shared_ptr<ec2::AbstractECConnection> messageBusConnection() const;

    /**
     * Manages which permissions User has on each of its accessible Resources.
     */
    QnResourceAccessManager* resourceAccessManager() const;

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

    // FIXME: #sivanov Remove compatibility layer.
    SystemSettings* globalSettings() const { return systemSettings(); }

    /**
     * Manages Showreels.
     */
    ShowreelManager* showreelManager() const;

    /**
     * Manages Lookup Lists. Initialized on the server side by default, on the desktop client only
     * after first use.  While not initialized, it is accessible but contains no data.
     */
    LookupListManager* lookupListManager() const;

    /**
     * List of all peers, connected to the System.
     */
    QnRuntimeInfoManager* runtimeInfoManager() const;

    /**
     * List of all User Groups.
     */
    UserGroupManager* userGroupManager() const;

    // TODO: #GDM Remove field.
    SystemContext* m_context = nullptr;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common
