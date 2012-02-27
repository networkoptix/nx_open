#ifndef __VIDEO_SERVER_H
#define __VIDEO_SERVER_H

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

    QnVideoServerConnectionPtr apiConnection();

    QnStorageResourceList getStorages() const;
    void setStorages(const QnStorageResourceList& storages);

private:
    QnVideoServerConnectionPtr m_restConnection;
    QString m_apiUrl;
    QnStorageResourceList m_storages;
};

typedef QnSharedResourcePointer<QnVideoServerResource> QnVideoServerResourcePtr;
typedef QList<QnVideoServerResourcePtr> QnVideoServerResourceList;

class QnVideoServerResourceFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
};

#endif // __VIDEO_SERVER_H
