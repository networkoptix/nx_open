// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QUrl>
#include <QtCore/QDateTime>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <nx/core/core_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/utils/value_cache.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/common/resource/resource_context.h>
#include <utils/common/instance_storage.h>

class QnStoragePluginFactory;
class QSettings;
class QnRouter;
class QnResourceDiscoveryManager;
class QnAuditManager;
class CameraDriverRestrictionList;

namespace nx::core::access { class ResourceAccessProvider; }
namespace nx::metrics { struct Storage; }
namespace nx::vms::common { class AbstractCertificateVerifier; }
namespace nx::vms::discovery { class Manager; }
namespace nx::vms::event { class RuleManager; }
namespace nx::vms::rules { class Engine; }

namespace nx::utils { class TimerManager; }

/**
 * Storage for the application singleton classes, which maintains correct order of the
 * initialization and destruction and provides access to all these singletons.
 *
 * One instance of Common Module corresponds to one application instance (Desktop or Mobile Client
 * or VMS Server). Functional tests can create several Common Module instances to emulate complex
 * System of several VMS Servers.
 *
 * As a temporary solution, QnCommonModule inherits ResourceContext for now. This is required to
 * simplify transition process.
 */
class NX_VMS_COMMON_API QnCommonModule:
    public QObject,
    public nx::vms::common::ResourceContext,
    public /*mixin*/ QnInstanceStorage
{
    Q_OBJECT

public:
    /**
     * Create singleton storage and a corresponding Resource Context.
     * @param clientMode Mode of the Module Discovery work.
     * @param resourceAccessMode Mode of the resource access permissions calculation.
     * @param peerId Id of the current peer in the Message Bus. It is persistent and is not changed
     *     between the application runs. It is stored in the application settings. VMS Server uses
     *     it as a Server Resource id. Desktop Client calculates actual peer id depending on the
     *     stored persistent id and on the number of the running client instance, so different
     *     Client windows have different peer ids.
     */
    explicit QnCommonModule(
        bool clientMode,
        nx::core::access::Mode resourceAccessMode,
        QnUuid peerId = QnUuid(),
        QObject* parent = nullptr);
    virtual ~QnCommonModule();

    using QnInstanceStorage::instance;
    using QnInstanceStorage::store;

    QnStoragePluginFactory* storagePluginFactory() const
    {
        return m_storagePluginFactory;
    }

    QnRouter* router() const;

    nx::vms::discovery::Manager* moduleDiscoveryManager() const;

    void setResourceDiscoveryManager(QnResourceDiscoveryManager* discoveryManager);

    QnResourceDiscoveryManager* resourceDiscoveryManager() const
    {
        return m_resourceDiscoveryManager;
    }

    nx::utils::TimerManager* timerManager() const { return m_timerManager.get(); }

    QnUuid dbId() const;
    void setDbId(const QnUuid& uuid);

    /*
    * This timestamp is using for database backup/restore operation.
    * Server has got systemIdentity time after DB restore operation
    * This time help pushing database from current server to all others
    */
    void setSystemIdentityTime(qint64 value, const QnUuid& sender);
    qint64 systemIdentityTime() const;

    nx::vms::api::ModuleInformation moduleInformation() const;

    bool isTranscodeDisabled() const { return m_transcodingDisabled; }
    void setTranscodeDisabled(bool value) { m_transcodingDisabled = value; }

    void setAllowedPeers(const QSet<QnUuid> &peerList) { m_allowedPeers = peerList; }
    QSet<QnUuid> allowedPeers() const { return m_allowedPeers; }

    QDateTime startupTime() const;

    QnUuid videowallGuid() const;
    void setVideowallGuid(const QnUuid &uuid);

    /**
     * Turn on/off connections to the remove peers.
     * Media server will not receive connections from another peers while it is disabled.
     * Hive mode is enabled by default.
     */
    void setStandAloneMode(bool value);
    bool isStandAloneMode() const;

    nx::utils::SoftwareVersion engineVersion() const;
    void setEngineVersion(const nx::utils::SoftwareVersion& version);

    nx::metrics::Storage* metrics() const;
    std::weak_ptr<nx::metrics::Storage> metricsWeakRef() const;

    void setAuditManager(QnAuditManager* auditManager);
    QnAuditManager* auditManager() const;

    /**
     * Set certificate verifier. Ownership is not controlled. Object must exist for all the class
     * lifetime.
     */
    void setCertificateVerifier(nx::vms::common::AbstractCertificateVerifier* value);

    nx::vms::common::AbstractCertificateVerifier* certificateVerifier() const;

    /** instanceCounter used for unit test purpose only */

    CameraDriverRestrictionList* cameraDriverRestrictionList() const;

signals:
    void systemIdentityTimeChanged(qint64 value, const QnUuid& sender);
    void standAloneModeChanged(bool value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
    std::unique_ptr<nx::utils::TimerManager> m_timerManager;
    std::shared_ptr<nx::metrics::Storage> m_metrics;

    const QString m_type;
    QnUuid m_dbId;
    mutable nx::Mutex m_mutex;
    bool m_transcodingDisabled = false;
    QSet<QnUuid> m_allowedPeers;
    qint64 m_systemIdentityTime = 0;

    QDateTime m_startupTime;

    QnStoragePluginFactory* m_storagePluginFactory = nullptr;

    QnResourceDiscoveryManager* m_resourceDiscoveryManager = nullptr;

    QnAuditManager* m_auditManager = nullptr;
    CameraDriverRestrictionList* m_cameraDriverRestrictionList = nullptr;

    QnUuid m_videowallGuid;
    bool m_standaloneMode = false;
    nx::utils::SoftwareVersion m_engineVersion;
};
