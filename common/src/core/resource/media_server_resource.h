#ifndef QN_MEDIA_SERVER_RESOURCE_H
#define QN_MEDIA_SERVER_RESOURCE_H

#include "core/resource/resource.h"
#include "core/resource/abstract_storage_resource.h"
#include "core/resource/media_resource.h"
#include "api/media_server_connection.h"

class QnLocalMediaServerResource : public QnResource
{
    Q_OBJECT

public:
    QnLocalMediaServerResource();

    virtual QString getUniqueId() const;

protected:

};


class QnMediaServerResource : public QnResource
{
    Q_OBJECT
    Q_PROPERTY(QString apiUrl READ getApiUrl WRITE setApiUrl)
    Q_PROPERTY(QString streamingUrl READ getStreamingUrl WRITE setStreamingUrl)

public:
    enum PanicMode {PM_None, PM_BusinessEvents, PM_User};

    QnMediaServerResource();
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

    virtual void updateInner(QnResourcePtr other) override;

    void determineOptimalNetIF();
    void setPrimaryIF(const QString& primaryIF);
    QString getPrimaryIF() const;

    void setReserve(bool reserve = true);
    bool getReserve() const;

    PanicMode getPanicMode() const;
    void setPanicMode(PanicMode panicMode);

    //virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);

    QString getProxyHost() const;
    int getProxyPort() const;

    QString getVersion() const;
    void setVersion(const QString& version);
private slots:
    void at_pingResponse(QnHTTPRawResponse, int);
signals:
    void serverIfFound(const QnMediaServerResourcePtr &resource, const QString &);
    void panicModeChanged(const QnMediaServerResourcePtr &resource);

private:
    QnMediaServerConnectionPtr m_restConnection;
    QString m_apiUrl;
    QString m_primaryIf;
    QString m_streamingUrl;
    QList<QHostAddress> m_netAddrList;
    QList<QHostAddress> m_prevNetAddrList;
    QnAbstractStorageResourceList m_storages;
    bool m_primaryIFSelected;
    bool m_reserve;
    PanicMode m_panicMode;
    QString m_version;
    QMap<int, QString> m_runningIfRequests;
};

class QnMediaServerResourceFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
};

Q_DECLARE_METATYPE(QnMediaServerResourcePtr);
Q_DECLARE_METATYPE(QnMediaServerResourceList);

#endif // QN_MEDIA_SERVER_RESOURCE_H
