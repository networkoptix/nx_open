#ifndef __VIDEO_SERVER_H
#define __VIDEO_SERVER_H

#include <QSharedPointer>
#include "core/resource/resource.h"
#include "core/resource/storage.h"
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

    QnVideoServerConnectionPtr apiConnection();

    void setGuid(const QString& guid);
    QString getGuid() const;

    QnStorageList getStorages() const;
    void setStorages(const QnStorageList& storages);

private:
    QnVideoServerConnectionPtr m_restConnection;
    QString m_apiUrl;
    QString m_guid;
    QnStorageList m_storages;
};


class QnVideoServerResourceFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
};

typedef QSharedPointer<QnVideoServerResource> QnVideoServerResourcePtr;
typedef QList<QnVideoServerResourcePtr> QnVideoServerResourceList;

#endif // __VIDEO_SERVER_H
