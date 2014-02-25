#ifndef QN_APP_SERVER_CONNECTION_H
#define QN_APP_SERVER_CONNECTION_H

#include <QtCore/QMutex>

#include <utils/common/request_param.h>

#include <core/resource/resource_fwd.h>
#include <core/misc/schedule_task.h>

#include <licensing/license.h>
#include <nx_ec/ec_api.h>

#include <api/model/servers_reply.h>
#include <api/model/connection_info.h>

#include "api_fwd.h"
#include "abstract_connection.h"

class QnAppServerConnectionFactory;
class QnApiSerializer;

class QN_EXPORT QnAppServerConnectionFactory 
{
public:
    QnAppServerConnectionFactory();
    virtual ~QnAppServerConnectionFactory();

    static QString authKey();
    static QString clientGuid();
    static QUrl defaultUrl();
    static QUrl publicUrl();
    static QByteArray prevSessionKey();
    static QByteArray sessionKey();
    static QString systemName();
    static int defaultMediaProxyPort();
    static QnSoftwareVersion currentVersion();
    static QnResourceFactory* defaultFactory();

    static void setAuthKey(const QString &key);
    static void setClientGuid(const QString &guid);
    static void setDefaultUrl(const QUrl &url);
    static void setDefaultFactory(QnResourceFactory *);
    static void setDefaultMediaProxyPort(int port);
    static void setCurrentVersion(const QnSoftwareVersion &version);
    static void setPublicIp(const QString &publicIp);
    static void setSystemName(const QString& systemName);

    static void setSessionKey(const QByteArray& sessionKey);

    //static QnAppServerConnectionPtr createConnection();
    //static QnAppServerConnectionPtr createConnection(const QUrl &url);

    static void setEC2ConnectionFactory( ec2::AbstractECConnectionFactory* ec2ConnectionFactory );
    static ec2::AbstractECConnectionFactory* ec2ConnectionFactory();
    static void setEc2Connection( ec2::AbstractECConnectionPtr connection );
    static ec2::AbstractECConnectionPtr getConnection2();

private:
    QMutex m_mutex;
    QString m_clientGuid;
    QString m_authKey;
    QUrl m_defaultUrl;
    QUrl m_publicUrl;
    QString m_systemName;
    QByteArray m_sessionKey;
    QByteArray m_prevSessionKey;

    int m_defaultMediaProxyPort;
    QnSoftwareVersion m_currentVersion;
    QnResourceFactory *m_resourceFactory;
};

bool initResourceTypes(ec2::AbstractECConnectionPtr ec2Connection);

#endif // QN_APP_SERVER_CONNECTION_H
