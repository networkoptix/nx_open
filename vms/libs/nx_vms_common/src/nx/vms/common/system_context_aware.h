// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

class QnCameraHistoryPool;
class QnCommonMessageProcessor;
class QnGlobalPermissionsManager;
class QnResourceAccessManager;
class QnResourcePool;
class QnResourcePropertyDictionary;
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
    ~SystemContextAware();

    /**
     * Linked context. May never be changed. Must exist when SystemContextAware is destroyed.
     */
    SystemContext* systemContext() const;

    /**
     * Id of the current peer in the Message Bus. It is persistent and is not changed between the
     * application runs. It is stored in the application settings. VMS Server uses it as a Server
     * Resource id. Desktop Client calculates actual peer id depending on the stored persistent id
     * and on the number of the running client instance, so different Client windows have different
     * peer ids.
     */
    QnUuid peerId() const;

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

    /**
     * Properties of all Resources in the System. Stored independently of Resources, can be loaded
     * earlier. Also can belong to the System as a whole (thus having null key).
     */
    QnResourcePropertyDictionary* resourcePropertyDictionary() const;

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
