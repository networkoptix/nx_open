// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QElapsedTimer>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource.h>
#include <nx/utils/value_cache.h>

#include <nx/network/socket_common.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/software_version.h>
#include <nx/utils/url.h>
#include <nx/utils/os_info.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/os_information.h>
#include <nx/vms/api/data/media_server_data.h>

namespace nx::network::http { class AsyncHttpClientPtr; }
namespace nx::core::resource::edge { class EdgeServerStateTracker; }

class NX_VMS_COMMON_API QnMediaServerResource:
    public QnResource,
    public /*mixin*/ Qn::EnableSafeDirectConnection
{
    Q_OBJECT

    typedef QnResource base_type;

public:
    QnMediaServerResource();
    virtual ~QnMediaServerResource();

    bool isEdgeServer() const;

    /** Overrides QnResource::getName. Returns server name from
     *  nx::vms::api::MediaServerUserAttributesData.
     * */
    virtual QString getName() const override;

    /** Overrides QnResource::setName. Writes name to nx::vms::api::MediaServerUserAttributesData.*/
    virtual void setName( const QString& name ) override;

    void setNetAddrList(const QList<nx::network::SocketAddress>& value);
    QList<nx::network::SocketAddress> getNetAddrList() const;

    // TODO: #dklychkov Use QSet instead of QList.
    void setAdditionalUrls(const QList<nx::utils::Url>& urls);
    QList<nx::utils::Url> getAdditionalUrls() const;

    void setIgnoredUrls(const QList<nx::utils::Url>& urls);
    QList<nx::utils::Url> getIgnoredUrls() const;

    std::optional<nx::network::SocketAddress> getCloudAddress() const;

    virtual QString getUrl() const override;
    virtual QString rtspUrl() const;
    virtual void setUrl(const QString& url) override;
    // TODO: #dklychkov Remove this, use getPrimaryAddress() instead.
    quint16 getPort() const;
    virtual nx::utils::Url getApiUrl() const;

    nx::network::SocketAddress getPrimaryAddress() const;
    void setPrimaryAddress(const nx::network::SocketAddress& getPrimaryAddress);

    /** Get the list of all available Server addresses. */
    QList<nx::network::SocketAddress> getAllAvailableAddresses() const;

    /** Online server with actual internet access. */
    bool hasInternetAccess() const;

    /** New Server Rest connection. */
    rest::ServerConnectionPtr restConnection() const;

    QnStorageResourceList getStorages() const;
    QnStorageResourcePtr getStorageByUrl(const QString& url) const;

    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

    Qn::PanicMode getPanicMode() const;
    void setPanicMode(Qn::PanicMode panicMode);

    nx::vms::api::ServerFlags getServerFlags() const;
    void setServerFlags(nx::vms::api::ServerFlags flags);

    int getMaxCameras() const;
    void setMaxCameras(int value);

    /**
     * Automatic failover moves Cameras across Servers with the same Location ID only.
     */
    void setLocationId(int value);
    int locationId() const;

    void setRedundancy(bool value);
    bool isRedundancy() const;

    void setCompatible(bool value);
    bool isCompatible() const;

    nx::utils::SoftwareVersion getVersion() const;
    void setVersion(const nx::utils::SoftwareVersion& version);

    nx::utils::OsInfo getOsInfo() const;
    void setOsInfo(const nx::utils::OsInfo& osInfo);

    virtual nx::vms::api::ModuleInformation getModuleInformation() const;
    nx::vms::api::ModuleInformationWithAddresses getModuleInformationWithAddresses() const;

    QString getAuthKey() const;
    void setAuthKey(const QString& value);

    /**
     * @return Data structure containing the backup bitrate limit in bytes per second for each hour
     *     for each day of week. Absence of a value for the given day-of-week/hour pair means that
     *     there is no limit for the corresponding time period.
     */
    nx::vms::api::BackupBitrateBytesPerSecond getBackupBitrateLimitsBps() const;
    void setBackupBitrateLimitsBps(const nx::vms::api::BackupBitrateBytesPerSecond& bitrateLimits);

    /** @return Realm to use in HTTP authentication. */
    QString realm() const;

    static bool isEdgeServer(const QnResourcePtr& resource);
    static bool isArmServer(const QnResourcePtr& resource);
    static bool isHiddenEdgeServer(const QnResourcePtr& resource);

    virtual void setStatus(
        nx::vms::api::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

    qint64 currentStatusTime() const;

    /** Server local timezone. */
    qint64 utcOffset(qint64 defaultValue = Qn::InvalidUtcOffset) const;
    void setUtcOffset(qint64 value);

    /**
     * This function is needed for the Client. Client may insert a fake Server with overridden Id
     * along with the reference to the original Id. So, the overridden method for the fake Server
     * returns its own non-original Id.
     */
    virtual QnUuid getOriginalGuid() const { return getId(); }

    /**
     * @return Ids of Analytics Engines which are actually running on the server.
     */
    QSet<QnUuid> activeAnalyticsEngineIds() const;

    QnUuid metadataStorageId() const;
    void setMetadataStorageId(const QnUuid& value);

    static constexpr qint64 kMinFailoverTimeoutMs = 1000 * 3;
    nx::vms::api::MediaServerUserAttributesData userAttributes() const;
    void setUserAttributes(const nx::vms::api::MediaServerUserAttributesData& attributes);
    bool setUserAttributesAndNotify(const nx::vms::api::MediaServerUserAttributesData& attributes);

    std::string certificate() const;
    std::string userProvidedCertificate() const;

    bool isWebCamerasDiscoveryEnabled() const;
    void setWebCamerasDiscoveryEnabled(bool value);
    bool isGuidConflictDetected() const;
    void setGuidConflictDetected(bool value);

private slots:
    void at_propertyChanged(const QnResourcePtr& /*res*/, const QString& key);
    void at_cloudSettingsChanged();

    void resetCachedValues();

signals:
    void serverFlagsChanged(const QnResourcePtr& resource);
    //! This signal is emitted when the set of additional URLs or ignored URLs has been changed.
    void auxUrlsChanged(const QnResourcePtr& resource);
    void versionChanged(const QnResourcePtr& resource);
    void redundancyChanged(const QnResourcePtr& resource);
    void backupScheduleChanged(const QnResourcePtr& resource);
    void apiUrlChanged(const QnResourcePtr& resource);
    void primaryAddressChanged(const QnResourcePtr& resource);
    void compatibilityChanged(const QnResourcePtr& resource);
    void edgeServerCanonicalStateChanged(const QnResourcePtr& resource);
    void certificateChanged(const QnMediaServerResourcePtr& server);
    void backupBitrateLimitsChanged(const QnResourcePtr& resource);
    void webCamerasDiscoveryChanged();
    void analyticsDescriptorsChanged();

protected:
    virtual void setSystemContext(nx::vms::common::SystemContext* systemContext) override;

private:
    Qn::PanicMode calculatePanicMode() const;

protected:
    mutable rest::ServerConnectionPtr m_restConnection;

private:
    nx::network::SocketAddress m_primaryAddress;
    QList<nx::network::SocketAddress> m_netAddrList;
    QList<nx::utils::Url> m_additionalUrls;
    QList<nx::utils::Url> m_ignoredUrls;
    nx::vms::api::ServerFlags m_serverFlags;
    nx::utils::SoftwareVersion m_version;
    nx::utils::OsInfo m_osInfo;
    QVector<nx::network::http::AsyncHttpClientPtr> m_runningIfRequests;
    QElapsedTimer m_statusTimer;
    QString m_authKey;
    bool m_isCompatible = true;
    mutable nx::Mutex m_attributesMutex;
    nx::vms::api::MediaServerUserAttributesData m_userAttributes;

    nx::utils::CachedValue<Qn::PanicMode> m_panicModeCache;

    // This extension is initialized only for Edge Servers.
    nx::core::resource::edge::EdgeServerStateTracker* edgeServerStateTracker();
    QScopedPointer<nx::core::resource::edge::EdgeServerStateTracker> m_edgeServerStateTracker;

    // TODO: #sivanov Move to the client code, it's used only there.
    std::atomic<qint64> m_utcOffset = Qn::InvalidUtcOffset;
};

Q_DECLARE_METATYPE(QnMediaServerResourcePtr)
Q_DECLARE_METATYPE(QnMediaServerResourceList)
