#ifndef QN_MEDIA_SERVER_RESOURCE_H
#define QN_MEDIA_SERVER_RESOURCE_H

#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>

#include <api/media_server_connection.h>

#include <utils/common/value_cache.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

#include <core/resource/resource.h>
#include <core/resource/resource_factory.h>
#include <core/resource/storage_resource.h>
#include "utils/network/http/asynchttpclient.h"


class QnMediaServerResource : public QnResource
{
    Q_OBJECT
    Q_PROPERTY(QString apiUrl READ getApiUrl WRITE setApiUrl)

public:
    QnMediaServerResource(const QnResourceTypePool* resTypePool);
    virtual ~QnMediaServerResource();

    virtual QString getUniqueId() const;

    //!Overrides \a QnResource::getName. Returns camera name (from \a QnCameraUserAttributes) of
    virtual QString getName() const override;
    //!Overrides \a QnResource::setName. Just calls \a QnSecurityCamResource::setCameraName
    /*!
        TODO get rid of this override, since mediaserver and client must call different methods (setName and setCameraName respectively)
    */
    virtual void setName( const QString& name ) override;
    void setServerName( const QString& name );

    void setApiUrl(const QString& apiUrl);
    QString getApiUrl() const;

    void setNetAddrList(const QList<QHostAddress>&);
    QList<QHostAddress> getNetAddrList() const;

    void setAdditionalUrls(const QList<QUrl> &urls);
    QList<QUrl> getAdditionalUrls() const;

    void setIgnoredUrls(const QList<QUrl> &urls);
    QList<QUrl> getIgnoredUrls() const;

    quint16 getPort() const;

    QnMediaServerConnectionPtr apiConnection();

    QnStorageResourceList getStorages() const;
    QnStorageResourcePtr getStorageByUrl(const QString& url) const;
    //void setStorages(const QnAbstractStorageResourceList& storages);

    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

    void setPrimaryAddress(const SocketAddress &primaryAddress);

    Qn::PanicMode getPanicMode() const;
    void setPanicMode(Qn::PanicMode panicMode);

    Qn::ServerFlags getServerFlags() const;
    void setServerFlags(Qn::ServerFlags flags);

    //virtual QnAbstractStreamDataProvider* createDataProviderInternal(Qn::ConnectionRole role);

    int getMaxCameras() const;
    void setMaxCameras(int value);

    void setRedundancy(bool value);
    bool isRedundancy() const;

    QnSoftwareVersion getVersion() const;
    void setVersion(const QnSoftwareVersion& version);

    QnSystemInformation getSystemInfo() const;
    void setSystemInfo(const QnSystemInformation &systemInfo);

    QString getSystemName() const;
    void setSystemName(const QString &systemName);

    QnModuleInformation getModuleInformation() const;

    QString getAuthKey() const;
    void setAuthKey(const QString& value);

    //!Returns realm to use in HTTP authentication
    QString realm() const;

    static bool isEdgeServer(const QnResourcePtr &resource);
    static bool isHiddenServer(const QnResourcePtr &resource);

    /** Original GUID is set for incompatible servers when their getGuid() getter returns a fake GUID.
     * This allows us to hold temporary fake server dublicates in the resource pool.
     */
    QnUuid getOriginalGuid() const;
    /** Set original GUID. No signals emmited after this method because the original GUID should not be changed after resource creation. */
    void setOriginalGuid(const QnUuid &guid);
    static bool isFakeServer(const QnResourcePtr &resource);

    virtual void setStatus(Qn::ResourceStatus newStatus, bool silenceMode = false) override;
    qint64 currentStatusTime() const;
    void setStorageDataToUpdate(const QnStorageResourceList& storagesToUpdate, const ec2::ApiIdDataList& storageUrlToRemove);
    /*
    * Return pair of handles of saving requests. first: handle for saving storages, second: handle for remove storages
    */
    QPair<int, int> saveUpdatedStorages();
    bool hasUpdatedStorages() const;
    void beforeDestroy();
private slots:
    void onNewResource(const QnResourcePtr &resource);
    void onRemoveResource(const QnResourcePtr &resource);
    void atResourceChanged();
    void at_propertyChanged(const QnResourcePtr & /*res*/, const QString & key);
private:
    void onRequestDone( int reqID, ec2::ErrorCode errorCode );
signals:
    void apiUrlChanged(const QnResourcePtr &resource);
    void serverFlagsChanged(const QnResourcePtr &resource);
    //! This signal is emmited when the set of additional URLs or ignored URLs has been changed.
    void auxUrlsChanged(const QnResourcePtr &resource);
    void versionChanged(const QnResourcePtr &resource);
    void systemNameChanged(const QnResourcePtr &resource);
    void redundancyChanged(const QnResourcePtr &resource);
    void storageSavingDone(int reqID, ec2::ErrorCode errCode);
private:
    QnMediaServerConnectionPtr m_restConnection;
    QString m_apiUrl;
    QList<QHostAddress> m_netAddrList;
    QList<QHostAddress> m_prevNetAddrList;
    QList<QUrl> m_additionalUrls;
    QList<QUrl> m_ignoredUrls;
    //QnAbstractStorageResourceList m_storages;
    Qn::ServerFlags m_serverFlags;
    QnSoftwareVersion m_version;
    QnSystemInformation m_systemInfo;
    QString m_systemName;
    QVector<nx_http::AsyncHttpClientPtr> m_runningIfRequests;
    QElapsedTimer m_statusTimer;
    QString m_authKey;

    QnUuid m_originalGuid;

    // used for client purpose only. Can be moved to separete class
    QnStorageResourceList m_storagesToUpdate;
    ec2::ApiIdDataList m_storagesToRemove;

    CachedValue<Qn::PanicMode> m_panicModeCache;

    mutable QnResourcePtr m_firstCamera;

    Qn::PanicMode calculatePanicMode() const;
};

class QnMediaServerResourceFactory : public QnResourceFactory
{
public:
    virtual QnResourcePtr createResource(const QnUuid& resourceTypeId, const QnResourceParams& params) override;
};

Q_DECLARE_METATYPE(QnMediaServerResourcePtr);
Q_DECLARE_METATYPE(QnMediaServerResourceList);

#endif // QN_MEDIA_SERVER_RESOURCE_H
