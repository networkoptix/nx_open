#ifndef QN_APP_SERVER_CONNECTION_H
#define QN_APP_SERVER_CONNECTION_H

#include <QtCore/QMutex>

#include <utils/common/request_param.h>
#include <utils/common/software_version.h>

#include <core/resource/resource_fwd.h>
#include <core/misc/schedule_task.h>

#include <licensing/license.h>
#include <nx_ec/ec_api.h>

#include <api/model/servers_reply.h>
#include <api/model/connection_info.h>

#include "api_fwd.h"

class QnAppServerConnectionFactory;
class QnApiSerializer;

class QN_EXPORT QnAppServerConnectionFactory 
{
public:
    QnAppServerConnectionFactory();
    virtual ~QnAppServerConnectionFactory();

    static QUrl url();
    static QnSoftwareVersion currentVersion();
    static QnResourceFactory* defaultFactory();

    static void setUrl(const QUrl &url);
    static void setDefaultFactory(QnResourceFactory *);
    static void setCurrentVersion(const QnSoftwareVersion &version);
    
    /** If the client is started in videowall mode, videowall's guid is stored here. */ 
    static QnUuid videowallGuid();
    static void setVideowallGuid(const QnUuid &uuid);

    /** If the client is started in videowall mode, instance's guid is stored here. */ 
    static QnUuid instanceGuid();
    static void setInstanceGuid(const QnUuid &uuid);

    //static QnAppServerConnectionPtr createConnection();
    //static QnAppServerConnectionPtr createConnection(const QUrl &url);

    static void setEC2ConnectionFactory( ec2::AbstractECConnectionFactory* ec2ConnectionFactory );
    static ec2::AbstractECConnectionFactory* ec2ConnectionFactory();
    static void setEc2Connection(const ec2::AbstractECConnectionPtr &connection );
    static ec2::AbstractECConnectionPtr getConnection2();

private:
    QMutex m_mutex;
    QUrl m_url;

    /** Videowall-related fields */
    QnUuid m_videowallGuid;
    QnUuid m_instanceGuid;

    QnSoftwareVersion m_currentVersion;
    QnResourceFactory *m_resourceFactory;
};

bool initResourceTypes(ec2::AbstractECConnectionPtr ec2Connection);

#endif // QN_APP_SERVER_CONNECTION_H
