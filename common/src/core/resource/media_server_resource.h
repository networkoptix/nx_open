#pragma once

#include <QtCore/QElapsedTimer>

#include <api/media_server_connection.h>

#include <utils/common/value_cache.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

#include <core/resource/resource.h>

#include "api/server_rest_connection_fwd.h"

namespace nx_http {
    class AsyncHttpClientPtr;
}
class SocketAddress;

class QnMediaServerResource : public QnResource
{
    Q_OBJECT

    typedef QnResource base_type;
public:
    QnMediaServerResource();
    virtual ~QnMediaServerResource();

    virtual QString getUniqueId() const;

    //!Overrides \a QnResource::getName. Returns camera name from \a QnMediaServerUserAttributes
    virtual QString getName() const override;

    //!Overrides \a QnResource::setName. Writes name to \a QnMediaServerUserAttributes
    virtual void setName( const QString& name ) override;

    void setNetAddrList(const QList<SocketAddress>& value);
    QList<SocketAddress> getNetAddrList() const;

    // TODO: #dklychkov Use QSet instead of QList
    void setAdditionalUrls(const QList<QUrl> &urls);
    QList<QUrl> getAdditionalUrls() const;

    void setIgnoredUrls(const QList<QUrl> &urls);
    QList<QUrl> getIgnoredUrls() const;

    quint16 getPort() const;

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

    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

    void setPrimaryAddress(const SocketAddress &primaryAddress);

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

    QString getSystemName() const;
    void setSystemName(const QString &systemName);

    QnModuleInformation getModuleInformation() const;
    void setFakeServerModuleInformation(const QnModuleInformationWithAddresses &moduleInformation);

    QString getAuthKey() const;
    void setAuthKey(const QString& value);

    QString getApiUrl() const;

    //!Returns realm to use in HTTP authentication
    QString realm() const;

    static bool isEdgeServer(const QnResourcePtr &resource);
    static bool isHiddenServer(const QnResourcePtr &resource);

    /** Original GUID is set for incompatible servers when their getGuid() getter returns a fake GUID.
     * This allows us to hold temporary fake server duplicates in the resource pool.
     */
    QnUuid getOriginalGuid() const;
    /** Set original GUID. No signals emmited after this method because the original GUID should not be changed after resource creation. */
    void setOriginalGuid(const QnUuid &guid);
    static bool isFakeServer(const QnResourcePtr &resource);

    virtual void setStatus(Qn::ResourceStatus newStatus, bool silenceMode = false) override;
    qint64 currentStatusTime() const;

    void beforeDestroy();

private slots:
    void onNewResource(const QnResourcePtr &resource);
    void onRemoveResource(const QnResourcePtr &resource);
    void atResourceChanged();
    void at_propertyChanged(const QnResourcePtr & /*res*/, const QString & key);
    void onPrimaryAddressChanged(const QnModuleInformation &moduleInformation, const SocketAddress &address);

signals:
    void portChanged(const QnResourcePtr &resource);
    void serverFlagsChanged(const QnResourcePtr &resource);
    //! This signal is emmited when the set of additional URLs or ignored URLs has been changed.
    void auxUrlsChanged(const QnResourcePtr &resource);
    void versionChanged(const QnResourcePtr &resource);
    void systemNameChanged(const QnResourcePtr &resource);
    void redundancyChanged(const QnResourcePtr &resource);
    void backupScheduleChanged(const QnResourcePtr &resource);
    void apiUrlChanged(const QnResourcePtr& resource);
    
private:
    QnMediaServerConnectionPtr m_apiConnection; // deprecated
    rest::QnConnectionPtr m_restConnection; // new one
    QList<SocketAddress> m_netAddrList;
    QList<QUrl> m_additionalUrls;
    QList<QUrl> m_ignoredUrls;
    Qn::ServerFlags m_serverFlags;
    QnSoftwareVersion m_version;
    QnSystemInformation m_systemInfo;
    QString m_systemName;
    QVector<nx_http::AsyncHttpClientPtr> m_runningIfRequests;
    QElapsedTimer m_statusTimer;
    QString m_authKey;

    QnUuid m_originalGuid;

    CachedValue<Qn::PanicMode> m_panicModeCache;

    mutable QnResourcePtr m_firstCamera;

    Qn::PanicMode calculatePanicMode() const;
};

Q_DECLARE_METATYPE(QnMediaServerResourcePtr);
Q_DECLARE_METATYPE(QnMediaServerResourceList);
