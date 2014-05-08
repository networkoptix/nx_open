#ifndef _server_appserver_processor_h_
#define _server_appserver_processor_h_

#include <nx_ec/ec_api.h>

#include "core/resource/resource.h"
#include "api/app_server_connection.h"
#include "mutex/distributed_mutex.h"

namespace ec2 {
    class QnMutexCameraDataHandler;
}

class QnAppserverResourceProcessor : public QObject, public QnResourceProcessor
{
    Q_OBJECT

public:
    QnAppserverResourceProcessor(QnId serverId);
    virtual ~QnAppserverResourceProcessor();

    void processResources(const QnResourceList &resources);

private:
    ec2::AbstractECConnectionPtr m_ec2Connection;
    QnId m_serverId;
    
    QSet<QnId> m_awaitingSetStatus;
    QSet<QnId> m_setStatusInProgress;

    struct LockData 
    {
        LockData() {}
        LockData(ec2::QnDistributedMutexPtr mutex, QnVirtualCameraResourcePtr cameraResource): mutex(mutex), cameraResource(cameraResource) {}

        ec2::QnDistributedMutexPtr mutex;
        QnVirtualCameraResourcePtr cameraResource;
    };
    QMap<QString, LockData> m_lockInProgress;
    ec2::QnMutexCameraDataHandler* m_cameraDataHandler;
    QMutex m_mutex;
private:
    void updateResourceStatusAsync(const QnResourcePtr &resource);
    bool isSetStatusInProgress(const QnResourcePtr &resource);
    void addNewCamera(QnVirtualCameraResourcePtr cameraResource);
    void addNewCameraInternal(QnVirtualCameraResourcePtr cameraResource);
private slots:
    void at_resource_statusChanged(const QnResourcePtr& resource);
    //void requestFinished(const QnHTTPRawResponse& response, int handle);
    void requestFinished2( int reqID, ec2::ErrorCode errCode, const QnId& id );

    void at_mutexLocked(QString name);
    void at_mutexTimeout(QString name);
};

#endif //_server_appserver_processor_h_
