#ifndef QN_VIDEO_SERVER_RESOURCE_H
#define QN_VIDEO_SERVER_RESOURCE_H

#include "core/resource/resource.h"
#include "core/resource/storage_resource.h"
#include "api/VideoServerConnection.h"

class QnLocalVideoServerResource : public QnResource
{
    Q_OBJECT

public:
    QnLocalVideoServerResource();

    virtual QString getUniqueId() const;
};


class QnVideoServerResource : public QnResource
{
    Q_OBJECT
    Q_PROPERTY(QString apiUrl READ getApiUrl WRITE setApiUrl)

public:
    QnVideoServerResource();
    virtual ~QnVideoServerResource();

    virtual QString getUniqueId() const;

    void setApiUrl(const QString& restUrl);
    QString getApiUrl() const;

    void setNetAddrList(const QList<QHostAddress>&);
    QList<QHostAddress> getNetAddrList();

    QnVideoServerConnectionPtr apiConnection();

    QnAbstractStorageResourceList getStorages() const;
    void setStorages(const QnAbstractStorageResourceList& storages);

    virtual void updateInner(QnResourcePtr other) override;

    void determineOptimalNetIF();
    void setPrimaryIF(const QString& primaryIF);
signals:
    void serverIFFound(QString);
private:
    QnVideoServerConnectionPtr m_restConnection;
    QString m_apiUrl;
    QList<QHostAddress> m_netAddrList;
    QList<QHostAddress> m_prevNetAddrList;
    QnAbstractStorageResourceList m_storages;
    bool m_primaryIFSelected;
};

class QnVideoServerResourceFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
};

Q_DECLARE_METATYPE(QnVideoServerResourcePtr);
Q_DECLARE_METATYPE(QnVideoServerResourceList);

#endif // QN_VIDEO_SERVER_RESOURCE_H
