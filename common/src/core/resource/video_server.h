#ifndef __VIDEO_SERVER_H
#define __VIDEO_SERVER_H

#include <QSharedPointer>
#include "core/resource/resource.h"
#include "api/VideoServerConnection.h"

class QnVideoServer: public QnResource
{
public:
    QnVideoServer();
    virtual ~QnVideoServer();
    virtual QString getUniqueId() const;
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);

    void setApiUrl(const QString& restUrl);
    QString getApiUrl() const;

    QnVideoServerConnectionPtr apiConnection();

private:
    QnVideoServerConnectionPtr m_restConnection;
    QString m_apiUrl;
};

class QnVideoServerFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(const QnId &resourceTypeId, const QnResourceParameters &parameters);
};

typedef QSharedPointer<QnVideoServer> QnVideoServerPtr;
typedef QList<QnVideoServerPtr> QnVideoServerList;

#endif
