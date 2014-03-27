#ifndef _server_appserver_processor_h_
#define _server_appserver_processor_h_

#include <nx_ec/ec_api.h>

#include "core/resource/resource.h"
#include "api/app_server_connection.h"

class QnAppserverResourceProcessor : public QObject, public QnResourceProcessor
{
    Q_OBJECT

public:
    QnAppserverResourceProcessor(QnId serverId);

    void processResources(const QnResourceList &resources);

private:
    ec2::AbstractECConnectionPtr m_ec2Connection;
    QnId m_serverId;
    
    QSet<QnId> m_awaitingSetStatus;
    QSet<QnId> m_setStatusInProgress;
    //QMap<int, QnResourcePtr> m_handleToResource;
private:
    void updateResourceStatusAsync(const QnResourcePtr &resource);
    bool isSetStatusInProgress(const QnResourcePtr &resource);
private slots:
    void at_resource_statusChanged(const QnResourcePtr& resource);
    //void requestFinished(const QnHTTPRawResponse& response, int handle);
    void requestFinished2( int reqID, ec2::ErrorCode errCode, const QnId& id );
};

#endif //_server_appserver_processor_h_
