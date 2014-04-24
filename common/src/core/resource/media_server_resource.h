#ifndef QN_MEDIA_SERVER_RESOURCE_H
#define QN_MEDIA_SERVER_RESOURCE_H

#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>

#include <api/media_server_connection.h>

#include <utils/common/software_version.h>

#include <core/resource/resource.h>
#include <core/resource/abstract_storage_resource.h>

class QnMediaServerResource : public QnResource
{
    Q_OBJECT
    Q_PROPERTY(QString apiUrl READ getApiUrl WRITE setApiUrl)
    Q_PROPERTY(QString streamingUrl READ getStreamingUrl WRITE setStreamingUrl)

public:

    QnMediaServerResource(const QnResourceTypePool* resTypePool);
    virtual ~QnMediaServerResource();

    virtual QString getUniqueId() const;

    void setApiUrl(const QString& restUrl);
    QString getApiUrl() const;

    void setStreamingUrl(const QString& value);
    QString getStreamingUrl() const;

    void setNetAddrList(const QList<QHostAddress>&);
    QList<QHostAddress> getNetAddrList();

    QnMediaServerConnectionPtr apiConnection();

    QnAbstractStorageResourceList getStorages() const;
    void setStorages(const QnAbstractStorageResourceList& storages);

    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

    void determineOptimalNetIF();
    void setPrimaryIF(const QString& primaryIF);
    QString getPrimaryIF() const;

    Qn::PanicMode getPanicMode() const;
    void setPanicMode(Qn::PanicMode panicMode);

    Qn::ServerFlags getServerFlags() const;
    void setServerFlags(Qn::ServerFlags flags);

    //virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);

    QString getProxyHost();
    int getProxyPort();

    int getMaxCameras() const;
    void setMaxCameras(int value);

    void setRedundancy(bool value);
    int isRedundancy() const;

    QnSoftwareVersion getVersion() const;
    void setVersion(const QnSoftwareVersion& version);
    static bool isEdgeServer(const QnResourcePtr &resource);
    virtual void setStatus(Status newStatus, bool silenceMode = false) override;
    qint64 currentStatusTime() const;
private slots:
    void at_pingResponse(QnHTTPRawResponse, int);
    void determineOptimalNetIF_testProxy();

signals:
    void serverIfFound(const QnMediaServerResourcePtr &resource, const QString &, const QString& );
    void panicModeChanged(const QnResourcePtr &resource);

private:
    QnMediaServerConnectionPtr m_restConnection;
    QString m_apiUrl;
    QString m_primaryIf;
    QString m_streamingUrl;
    QList<QHostAddress> m_netAddrList;
    QList<QHostAddress> m_prevNetAddrList;
    QnAbstractStorageResourceList m_storages;
    bool m_primaryIFSelected;
    Qn::ServerFlags m_serverFlags;
    Qn::PanicMode m_panicMode;
    QnSoftwareVersion m_version;
    QMap<int, QString> m_runningIfRequests;
    QObject *m_guard; // TODO: #Elric evil hack. Remove once roma's direct connection hell is refactored out.
    int m_maxCameras;
    bool m_redundancy;
    QElapsedTimer m_statusTimer;
};

class QnMediaServerResourceFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParams& params);
};

Q_DECLARE_METATYPE(QnMediaServerResourcePtr);
Q_DECLARE_METATYPE(QnMediaServerResourceList);

#endif // QN_MEDIA_SERVER_RESOURCE_H
