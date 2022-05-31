// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/core/access/access_types.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "system_context_aware.h"  //< Forward declarations.

class QnGlobalPermissionsManager;
class QnLayoutTourManager;
class QnLicensePool;
class QnResourceAccessSubjectsCache;
class QnResourceDataPool;
class QnResourcePropertyDictionary;
class QnResourceStatusDictionary;
class QnRouter;
class QnRuntimeInfoManager;
class QnSharedResourcesManager;
class QnServerAdditionalAddressesDictionary;
class QnUserRolesManager;

namespace nx::analytics::taxonomy {
class AbstractState;
class AbstractStateWatcher;
class DescriptorContainer;
} // namespace nx::analytics::taxonomy;

namespace ec2 { class AbstractECConnection; }
namespace nx::core::access { class ResourceAccessProvider; }
namespace nx::vms::event { class RuleManager; }
namespace nx::vms::rules { class Engine; }

namespace nx::vms::common {

class AbstractCertificateVerifier;
class SystemSettings;

/**
 * Storage for the application Resource Context classes. One Resource Context corresponds to one
 * client-server connection. Also there can be separate Resource Context for the local files in the
 * Desktop Client. VMS Server has the only one Resource Context for it's own database connection.
 */
class NX_VMS_COMMON_API SystemContext: public QObject
{
    Q_OBJECT

public:
    /**
     * @param peerId Id of the current peer in the Message Bus. It is persistent and is not changed
     *     between the application runs. See ::peerId() for the details.
     * @param sessionId Id of the connection session. On the server side it is randomly generated
     *     on the server start. On the client side it is generated separately for each connection.
     * @param resourceAccessMode Mode of the Resource permissions mechanism work. Direct mode is
     *     used on the Server side, all calculations occur on the fly. Cached mode is used on the
     *     Client side, where we need to actively listen for the changes and emit signals.
     */
    SystemContext(
        QnUuid peerId,
        QnUuid sessionId,
        nx::core::access::Mode resourceAccessMode,
        QObject* parent = nullptr);
    virtual ~SystemContext();

    /**
     * Enable network-related functionality. Can be disabled in unit tests.
     */
    void initNetworking(QnRouter* router, AbstractCertificateVerifier* certificateVerifier);

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
     * Helper class to find the shortest route to the VMS Server. Can be null, e.g. in unit tests.
     */
    QnRouter* router() const;

    /**
     * Interface to create SSL certificate validation functors.
     */
    AbstractCertificateVerifier* certificateVerifier() const;

    /**
     * Interface for the Message Bus connection.
     */
    std::shared_ptr<ec2::AbstractECConnection> ec2Connection() const;

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
     * List of all User Roles.
     */
    QnUserRolesManager* userRolesManager() const;

    /**
     * Manages which Resources are directly shared with Users or Roles.
     */
    QnSharedResourcesManager* sharedResourcesManager() const;

    /**
     * Manages which permissions User has on each of its accessible Resources.
     */
    QnResourceAccessManager* resourceAccessManager() const;

    /**
     * Manages which global permissions each User or Role has.
     */
    QnGlobalPermissionsManager* globalPermissionsManager() const;

    /**
     * Cache of Users by Roles.
     */
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;

    /**
     * Manages which Resources are accessible to Users and Roles.
     */
    nx::core::access::ResourceAccessProvider* resourceAccessProvider() const;

    /**
     * Manages Showreels.
     */
    QnLayoutTourManager* layoutTourManager() const;

    /**
     * Manages old event rules.
     */
    nx::vms::event::RuleManager* eventRuleManager() const;

    nx::analytics::taxonomy::DescriptorContainer* analyticsDescriptorContainer() const;

    nx::analytics::taxonomy::AbstractStateWatcher* analyticsTaxonomyStateWatcher() const;

    std::shared_ptr<nx::analytics::taxonomy::AbstractState> analyticsTaxonomyState() const;

protected:
    virtual void setMessageProcessor(QnCommonMessageProcessor* messageProcessor);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common
