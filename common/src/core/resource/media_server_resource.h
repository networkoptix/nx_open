#ifndef QN_MEDIA_SERVER_RESOURCE_H
#define QN_MEDIA_SERVER_RESOURCE_H

#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>

#include <api/media_server_connection.h>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

#include <core/resource/resource.h>
#include <core/resource/resource_factory.h>
#include <core/resource/abstract_storage_resource.h>


class QnMediaServerResource : public QnResource
{
    Q_OBJECT
    Q_PROPERTY(QString apiUrl READ getApiUrl WRITE setApiUrl)

public:
    static const QString USE_PROXY;

    QnMediaServerResource(const QnResourceTypePool* resTypePool);
    virtual ~QnMediaServerResource();

    virtual QString getUniqueId() const;

    void setApiUrl(const QString& restUrl);
    QString getApiUrl() const;

    void setNetAddrList(const QList<QHostAddress>&);
    const QList<QHostAddress>& getNetAddrList() const;

    void setAdditionalUrls(const QList<QUrl> &urls);
    QList<QUrl> getAdditionalUrls() const;

    void setIgnoredUrls(const QList<QUrl> &urls);
    QList<QUrl> getIgnoredUrls() const;

    QnMediaServerConnectionPtr apiConnection();

    QnAbstractStorageResourceList getStorages() const;
    void setStorages(const QnAbstractStorageResourceList& storages);

    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

    void determineOptimalNetIF();
    void setPrimaryIF(const QString& primaryIF);
    /*!
        \return If there is route to mediaserver, ip address of mediaserver. If there is no route, string \a USE_PROXY
    */
    QString getPrimaryIF() const;

    Qn::PanicMode getPanicMode() const;
    void setPanicMode(Qn::PanicMode panicMode);

    Qn::ServerFlags getServerFlags() const;
    void setServerFlags(Qn::ServerFlags flags);

    //virtual QnAbstractStreamDataProvider* createDataProviderInternal(Qn::ConnectionRole role);

    int getMaxCameras() const;
    void setMaxCameras(int value);

    void setRedundancy(bool value);
    int isRedundancy() const;

    QnSoftwareVersion getVersion() const;
    void setVersion(const QnSoftwareVersion& version);

    QnSystemInformation getSystemInfo() const;
    void setSystemInfo(const QnSystemInformation &systemInfo);

    QString getSystemName() const;
    void setSystemName(const QString &systemName);

    QnModuleInformation getModuleInformation() const;

    QString getAuthKey() const;
    void setAuthKey(const QString& value);

    static bool isHiddenServer(const QnResourcePtr &resource);
    virtual void setStatus(Qn::ResourceStatus newStatus, bool silenceMode = false) override;
    qint64 currentStatusTime() const;
private slots:
    void at_pingResponse(QnHTTPRawResponse, int);

signals:
    void serverIfFound(const QnMediaServerResourcePtr &resource, const QString &, const QString& );
    void panicModeChanged(const QnResourcePtr &resource);
    //! This signal is emmited when the set of additional URLs or ignored URLs has been changed.
    void auxUrlsChanged(const QnResourcePtr &resource);
    void versionChanged(const QnResourcePtr &resource);
    void systemNameChanged(const QnResourcePtr &resource);
    void redundancyChanged(const QnResourcePtr &resource);

private:
    QnMediaServerConnectionPtr m_restConnection;
    QString m_apiUrl;
    QString m_primaryIf;
    QList<QHostAddress> m_netAddrList;
    QList<QHostAddress> m_prevNetAddrList;
    QList<QUrl> m_additionalUrls;
    QList<QUrl> m_ignoredUrls;
    QnAbstractStorageResourceList m_storages;
    bool m_primaryIFSelected;
    Qn::ServerFlags m_serverFlags;
    Qn::PanicMode m_panicMode;
    QnSoftwareVersion m_version;
    QnSystemInformation m_systemInfo;
    QString m_systemName;
    QMap<int, QString> m_runningIfRequests;
    QObject *m_guard; // TODO: #Elric evil hack. Remove once roma's direct connection hell is refactored out.
    int m_maxCameras;
    bool m_redundancy;
    QElapsedTimer m_statusTimer;
    QString m_authKey;
};

class QnMediaServerResourceFactory : public QnResourceFactory
{
public:
    virtual QnResourcePtr createResource(const QnUuid& resourceTypeId, const QnResourceParams& params) override;
};

Q_DECLARE_METATYPE(QnMediaServerResourcePtr);
Q_DECLARE_METATYPE(QnMediaServerResourceList);

#endif // QN_MEDIA_SERVER_RESOURCE_H
