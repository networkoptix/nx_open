#pragma once

#include <nx/utils/thread/mutex.h>

#include <utils/common/request_param.h>

#include <core/resource/resource_fwd.h>
#include <core/misc/schedule_task.h>

#include <licensing/license.h>
#include <nx_ec/ec_api.h>

#include <api/model/servers_reply.h>
#include <api/model/connection_info.h>

class QnAppServerConnectionFactory;
class QnApiSerializer;

class QN_EXPORT QnAppServerConnectionFactory
{
public:
    QnAppServerConnectionFactory();
    virtual ~QnAppServerConnectionFactory();

    static QUrl url();
    static QnResourceFactory* defaultFactory();

    static void setUrl(const QUrl &url);
    static void setDefaultFactory(QnResourceFactory *);

    /** If the client is started in videowall mode, videowall's guid is stored here. */
    static QnUuid videowallGuid();
    static void setVideowallGuid(const QnUuid &uuid);

    /** If the client is started in videowall mode, instance's guid is stored here. */
    static QnUuid instanceGuid();
    static void setInstanceGuid(const QnUuid &uuid);

    static QnConnectionInfo connectionInfo();
    static void setConnectionInfo(const QnConnectionInfo& connectionInfo);

    static void setEC2ConnectionFactory( ec2::AbstractECConnectionFactory* ec2ConnectionFactory );
    static ec2::AbstractECConnectionFactory* ec2ConnectionFactory();
    static void setEc2Connection(const ec2::AbstractECConnectionPtr &connection );
    static ec2::AbstractECConnectionPtr getConnection2();

private:
    QnMutex m_mutex;
    QUrl m_url;

    /** Videowall-related fields */
    QnUuid m_videowallGuid;
    QnUuid m_instanceGuid;

    QnConnectionInfo m_connectionInfo;

    QnResourceFactory *m_resourceFactory;
};
