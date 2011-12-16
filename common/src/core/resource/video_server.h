#ifndef __VIDEO_SERVER_H
#define __VIDEO_SERVER_H

#include <QSharedPointer>
#include "core/resource/resource.h"
#include "api/VideoServerConnection.h"

class QnLocalVideoServer : public QnResource
{
public:
    QnLocalVideoServer();
    virtual ~QnLocalVideoServer();

    virtual QString getUniqueId() const;
    virtual QnAbstractStreamDataProvider *createDataProviderInternal(ConnectionRole role);
};


class QnVideoServer : public QnResource
{
    Q_OBJECT
    Q_PROPERTY(QString apiUrl READ getApiUrl WRITE setApiUrl)

public:
    QnVideoServer();
    virtual ~QnVideoServer();

    virtual QString getUniqueId() const;
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);

    void setApiUrl(const QString& restUrl);
    QString getApiUrl() const;

    QnVideoServerConnectionPtr apiConnection();

    void setGuid(const QString& guid);
    QString getGuid() const;

private:
    QnVideoServerConnectionPtr m_restConnection;
    QString m_apiUrl;
    QString m_guid;
};


class QnVideoServerFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(const QnId &resourceTypeId, const QnResourceParameters &parameters);
};

typedef QSharedPointer<QnVideoServer> QnVideoServerPtr;
typedef QList<QnVideoServerPtr> QnVideoServerList;

#endif // __VIDEO_SERVER_H
