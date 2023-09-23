// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

namespace nx::vms::common {

class AbstractCertificateVerifier;
class SystemContext;

} // namespace nx::vms::common

namespace nx::metrics { struct Storage; }
namespace nx::vms::discovery { class Manager; }
namespace nx::vms::event { class RuleManager; }

/**
 * Deprecated storage for the application singleton classes. Should not be used anymore. Use
 * SystemContext or ApplicationContext, depending on the module usage scope.
 *
 * As a temporary solution, QnCommonModule stores SystemContext for now. This is required to
 * simplify transition process.
 */
class NX_VMS_COMMON_API QnCommonModule: public QObject
{
    Q_OBJECT

public:
    /**
     * Deprecated singleton storage. Should
     * @param systemContext General System Context. Ownership is managed by the caller.
     */
    explicit QnCommonModule(nx::vms::common::SystemContext* systemContext);
    virtual ~QnCommonModule();

    nx::vms::common::SystemContext* systemContext() const;

    //---------------------------------------------------------------------------------------------
    // Temporary methods for the migration simplification.
    nx::vms::common::AbstractCertificateVerifier* certificateVerifier() const;
    QnUuid peerId() const;
    QnUuid sessionId() const;
    QnLicensePool* licensePool() const;
    QnRuntimeInfoManager* runtimeInfoManager() const;
    QnResourcePool* resourcePool() const;
    QnResourceAccessManager* resourceAccessManager() const;
    QnCameraHistoryPool* cameraHistoryPool() const;
    QnResourcePropertyDictionary* resourcePropertyDictionary() const;
    QnResourceStatusDictionary* resourceStatusDictionary() const;
    nx::vms::common::SystemSettings* globalSettings() const;
    nx::vms::event::RuleManager* eventRuleManager() const;
    QnResourceDataPool* resourceDataPool() const;
    std::shared_ptr<ec2::AbstractECConnection> messageBusConnection() const;
    QnCommonMessageProcessor* messageProcessor() const;
    nx::metrics::Storage* metrics() const;
    std::weak_ptr<nx::metrics::Storage> metricsWeakRef() const;
    //---------------------------------------------------------------------------------------------

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
