// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/core/access/access_types.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "system_context_aware.h" //< Forward declarations.

class QnCameraNamesWatcher;
class QnLicensePool;
class QnResourceDataPool;
class QnResourcePropertyDictionary;
class QnResourceStatusDictionary;
class QnRouter;
class QnServerAdditionalAddressesDictionary;

namespace nx::analytics::taxonomy {
class AbstractState;
class AbstractStateWatcher;
class DescriptorContainer;
} // namespace nx::analytics::taxonomy;

namespace nx::core::access {
class AccessRightsManager;
class GlobalPermissionsWatcher;
class ResourceAccessSubjectHierarchy;
} // namespace nx::core::access

namespace nx::metrics { struct Storage; }
namespace nx::vms::discovery { class Manager; }
namespace nx::vms::event { class RuleManager; }
namespace nx::vms::rules { class Engine; }

namespace nx::vms::common {

class AbstractCertificateVerifier;
class DeviceLicenseUsageWatcher;
class VideoWallLicenseUsageWatcher;
class SystemSettings;
class UserGroupManager;

namespace saas { class ServiceManager; }

/**
 * Storage for the application Resource Context classes. One Resource Context corresponds to one
 * client-server connection. Also there can be separate Resource Context for the local files in the
 * Desktop Client. VMS Server has the only one Resource Context for it's own database connection.
 */
class NX_VMS_COMMON_API SystemContext: public QObject
{
    Q_OBJECT

public:
    /** Various types of system contexts. Some of them require shortened initialization. */
    enum class Mode
    {
        /** Generic System Context for the Desktop or Mobile Client. */
        client,

        /** Generic System Context for the Server. */
        server,

        /** System Context for the cross-system connection. */
        crossSystem,

        /** System Context for the cloud layouts storage. */
        cloudLayouts,

        /** System Context for the unit tests. */
        unitTests,

        // There will be separate type for the local files context in the future.
    };

    /**
     * @param peerId Id of the current peer in the Message Bus. It is persistent and is not changed
     *     between the application runs. See ::peerId() for the details.
     * @param sessionId Id of the connection session. On the server side it is randomly generated
     *     on the server start. On the client side it is generated separately for each connection.
     * @param resourceAccessMode Mode of the Resource permissions mechanism work. Direct mode is
     *     used on the Server side, all calculations occur on the fly. Cached mode is used for the
     *     current context on the Client side, where we need to actively listen for the changes and
     *     emit signals. Cross-system contexts also use direct mode.
     */
    SystemContext(
        Mode mode,
        QnUuid peerId,
        QnUuid sessionId,
        nx::core::access::Mode resourceAccessMode,
        QObject* parent = nullptr);
    virtual ~SystemContext();

    /**
     * Id of the current peer in the Message Bus. It is persistent and is not changed between the
     * application runs. It is stored in the application settings. VMS Server uses it as a Server
     * Resource id. Desktop Client calculates actual peer id depending on the stored persistent id
     * and on the number of the running client instance, so different Client windows have different
     * peer ids.
     */
    const QnUuid& peerId() const;

    /**
     * Id of the connection session. On the server side it is randomly generated on the server
     * start. On the client side it is generated separately for each connection.
     */
    const QnUuid& sessionId() const;

    /** Temporary method to simplify refactor. */
    void updateRunningInstanceGuid();

    /**
     * Enable network-related functionality. Can be disabled in unit tests.
     */
    void enableNetworking(AbstractCertificateVerifier* certificateVerifier);

    /**
     * Interface to create SSL certificate validation functors.
     */
    AbstractCertificateVerifier* certificateVerifier() const;

    /**
     * Enable access to Context's servers using alternative routes, found by the provided manager.
     */
    void enableRouting(nx::vms::discovery::Manager* moduleDiscoveryManager);

    /**
     * Whether Module Discovery Manager is set to the context.
     */
    bool isRoutingEnabled() const;

    /**
     * Helper class to find the shortest route to the VMS Server. Can be null, e.g. in unit tests
     * or in the cross-System contexts.
     */
    nx::vms::discovery::Manager* moduleDiscoveryManager() const;

    /**
     * Interface for the Message Bus connection.
     */
    std::shared_ptr<ec2::AbstractECConnection> messageBusConnection() const;

    QnCommonMessageProcessor* messageProcessor() const;

    template <class MessageProcessorType, typename ...Args>
    MessageProcessorType* createMessageProcessor(Args... args)
    {
        const auto processor = new MessageProcessorType(this, args...);
        setMessageProcessor(processor);
        return processor;
    }

    void deleteMessageProcessor();

    /**
     * List of all licenses, installed in the System.
     */
    QnLicensePool* licensePool() const;

    saas::ServiceManager* saasServiceManager() const;

    /** Watches and notifies when Device license usage is changed in the System. */
    DeviceLicenseUsageWatcher* deviceLicenseUsageWatcher() const;

    /** Watches and notifies when Video Wall license usage is changed in the System. */
    VideoWallLicenseUsageWatcher* videoWallLicenseUsageWatcher() const;

    /**
     * List of all Resources in the System. Some data is stored in the external dictionaries.
     */
    QnResourcePool* resourcePool() const;

    /**
     * Information about default Device property values based on Device model.
     */
    QnResourceDataPool* resourceDataPool() const;

    /**
     * Resource external data: status.
     */
    QnResourceStatusDictionary* resourceStatusDictionary() const;

    /**
     * Resource external data: properties.
     */
    QnResourcePropertyDictionary* resourcePropertyDictionary() const;

    /**
     * Information about Servers, storing Device footage.
     */
    QnCameraHistoryPool* cameraHistoryPool() const;

    /**
     * Resource external data: Server additional addresses.
     */
    QnServerAdditionalAddressesDictionary* serverAdditionalAddressesDictionary() const;

    /**
     * List of all peers, connected to the System.
     */
    QnRuntimeInfoManager* runtimeInfoManager() const;

    /**
     * System settings, which do not depend on any Device or Server and are applied globally.
     * Currently stored as Resource Properties for the `admin` User.
     */
    SystemSettings* globalSettings() const;

    /**
     * List of all User Groups.
     */
    UserGroupManager* userGroupManager() const;

    /**
     * Manages which Resources are directly shared with Users or Roles.
     */
    nx::core::access::AccessRightsManager* accessRightsManager() const;

    /**
     * Keeps track of User and User Group own global permissions and notifies about their changes.
     */
    nx::core::access::GlobalPermissionsWatcher* globalPermissionsWatcher() const;

    /**
     * Manages which permissions User has on each of its accessible Resources.
     */
    QnResourceAccessManager* resourceAccessManager() const;

    /**
     * Manages relations between Users and Groups.
     */
    nx::core::access::ResourceAccessSubjectHierarchy* accessSubjectHierarchy() const;

    /**
     * Manages Showreels.
     */
    ShowreelManager* showreelManager() const;

    /**
     * Manages Lookup Lists. Initialized on the server side by default, on the desktop client only
     * after first use. While not initialized, it is accessible but contains no data.
     */
    LookupListManager* lookupListManager() const;

    /**
     * Manages old event rules.
     */
    nx::vms::event::RuleManager* eventRuleManager() const;

    nx::vms::rules::Engine* vmsRulesEngine() const;
    void setVmsRulesEngine(nx::vms::rules::Engine* engine);

    nx::analytics::taxonomy::DescriptorContainer* analyticsDescriptorContainer() const;

    nx::analytics::taxonomy::AbstractStateWatcher* analyticsTaxonomyStateWatcher() const;

    std::shared_ptr<nx::analytics::taxonomy::AbstractState> analyticsTaxonomyState() const;

    std::shared_ptr<nx::metrics::Storage> metrics() const;

    QnCameraNamesWatcher* cameraNamesWatcher() const;

protected:
    Mode mode() const;

    virtual void setMessageProcessor(QnCommonMessageProcessor* messageProcessor);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common
