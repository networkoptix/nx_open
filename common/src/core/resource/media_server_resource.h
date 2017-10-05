#pragma once

#include <QtCore/QElapsedTimer>

#include <api/media_server_connection.h>
#include <utils/common/value_cache.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <nx/utils/safe_direct_connection.h>
#include <core/resource/resource.h>
#include <nx/network/socket_common.h>

#include "api/server_rest_connection_fwd.h"

namespace nx_http { class AsyncHttpClientPtr; }
namespace nx { namespace api { struct AnalyticsDriverManifest; } }

class SocketAddress;

class QnMediaServerResource:
    public QnResource,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

    typedef QnResource base_type;
public:
    QnMediaServerResource(QnCommonModule* commonModule);
    virtual ~QnMediaServerResource();

    virtual QString getUniqueId() const;

    //!Overrides \a QnResource::getName. Returns camera name from \a QnMediaServerUserAttributes
    virtual QString getName() const override;

    virtual QString toSearchString() const override;

    //!Overrides \a QnResource::setName. Writes name to \a QnMediaServerUserAttributes
    virtual void setName( const QString& name ) override;

    void setNetAddrList(const QList<SocketAddress>& value);
    QList<SocketAddress> getNetAddrList() const;

    // TODO: #dklychkov Use QSet instead of QList
    void setAdditionalUrls(const QList<QUrl> &urls);
    QList<QUrl> getAdditionalUrls() const;

    void setIgnoredUrls(const QList<QUrl> &urls);
    QList<QUrl> getIgnoredUrls() const;

    boost::optional<SocketAddress> getCloudAddress() const;

    virtual QString getUrl() const override;
    virtual void setUrl(const QString& url) override;
    // TODO: #dklychkov remove this, use getPrimaryAddress() instead.
    quint16 getPort() const;
    virtual nx::utils::Url getApiUrl() const;

    SocketAddress getPrimaryAddress() const;
    void setPrimaryAddress(const SocketAddress &getPrimaryAddress);

    bool isSslAllowed() const;
    void setSslAllowed(bool sslAllowed);

    /** Get list of all available server addresses. */
    QList<SocketAddress> getAllAvailableAddresses() const;

    /*
    * Deprecated server rest connection
    */
    QnMediaServerConnectionPtr apiConnection();

    /*
    * New server rest connection
    */
    rest::QnConnectionPtr restConnection();

    QnStorageResourceList getStorages() const;
    QnStorageResourcePtr getStorageByUrl(const QString& url) const;

    virtual void updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers) override;

    Qn::PanicMode getPanicMode() const;
    void setPanicMode(Qn::PanicMode panicMode);

    Qn::ServerFlags getServerFlags() const;
    void setServerFlags(Qn::ServerFlags flags);

    int getMaxCameras() const;
    void setMaxCameras(int value);

    void setRedundancy(bool value);
    bool isRedundancy() const;

    QnServerBackupSchedule getBackupSchedule() const;
    void setBackupSchedule(const QnServerBackupSchedule &value);

    QnSoftwareVersion getVersion() const;
    void setVersion(const QnSoftwareVersion& version);

    QnSystemInformation getSystemInfo() const;
    void setSystemInfo(const QnSystemInformation &systemInfo);
    virtual QnModuleInformation getModuleInformation() const;

    QnModuleInformationWithAddresses getModuleInformationWithAddresses() const;

    QList<nx::api::AnalyticsDriverManifest> analyticsDrivers() const;
    void setAnalyticsDrivers(const QList<nx::api::AnalyticsDriverManifest>& drivers);

    QString getAuthKey() const;
    void setAuthKey(const QString& value);

    virtual void setResourcePool(QnResourcePool *resourcePool) override;

    //!Returns realm to use in HTTP authentication
    QString realm() const;

    static bool isEdgeServer(const QnResourcePtr &resource);
    static bool isHiddenServer(const QnResourcePtr &resource);

    virtual void setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;
    qint64 currentStatusTime() const;

    void beforeDestroy();

    /**
     * This function need for client. Client may insert fakeServer with overriden ID and
     * reference to a original ID. So, its overrid this call for fakeMediaServer
     */
    virtual QnUuid getOriginalGuid() const { return getId();  }

    static constexpr qint64 kMinFailoverTimeoutMs = 1000 * 3;

protected:
    static QString apiUrlScheme(bool sslAllowed);

private slots:
    void onNewResource(const QnResourcePtr &resource);
    void onRemoveResource(const QnResourcePtr &resource);
    void at_propertyChanged(const QnResourcePtr & /*res*/, const QString & key);
    void at_cloudSettingsChanged();

    void resetCachedValues();

signals:
    void portChanged(const QnResourcePtr &resource);
    void serverFlagsChanged(const QnResourcePtr &resource);
    //! This signal is emmited when the set of additional URLs or ignored URLs has been changed.
    void auxUrlsChanged(const QnResourcePtr &resource);
    void versionChanged(const QnResourcePtr &resource);
    void redundancyChanged(const QnResourcePtr &resource);
    void backupScheduleChanged(const QnResourcePtr &resource);
    void apiUrlChanged(const QnResourcePtr& resource);
    void primaryAddressChanged(const QnResourcePtr& resource);
private:
    SocketAddress m_primaryAddress;
    QnMediaServerConnectionPtr m_apiConnection; // deprecated
    rest::QnConnectionPtr m_restConnection; // new one
    QList<SocketAddress> m_netAddrList;
    QList<QUrl> m_additionalUrls;
    QList<QUrl> m_ignoredUrls;
    bool m_sslAllowed = false;
    Qn::ServerFlags m_serverFlags;
    QnSoftwareVersion m_version;
    QnSystemInformation m_systemInfo;
    QVector<nx_http::AsyncHttpClientPtr> m_runningIfRequests;
    QElapsedTimer m_statusTimer;
    QString m_authKey;

    CachedValue<Qn::PanicMode> m_panicModeCache;
    CachedValue<QList<nx::api::AnalyticsDriverManifest>> m_analyticsDriversCache;

    mutable QnResourcePtr m_firstCamera;

    Qn::PanicMode calculatePanicMode() const;
    nx::utils::Url buildApiUrl() const;
};

Q_DECLARE_METATYPE(QnMediaServerResourcePtr)
Q_DECLARE_METATYPE(QnMediaServerResourceList)
