#ifndef _server_appserver_processor_h_
#define _server_appserver_processor_h_

#include <nx_ec/ec_api.h>

#include <core/resource/resource.h>
#include <core/resource/resource_processor.h>

#include "api/app_server_connection.h"
#include "mutex/distributed_mutex.h"

namespace ec2 {
    class QnMutexCameraDataHandler;
}

class QnAppserverResourceProcessor : public QObject, public QnResourceProcessor
{
    Q_OBJECT

public:
    QnAppserverResourceProcessor(QUuid serverId);
    virtual ~QnAppserverResourceProcessor();

    virtual bool isBusy() const override;
    virtual void processResources(const QnResourceList &resources) override;

private:
    ec2::AbstractECConnectionPtr m_ec2Connection;
    QUuid m_serverId;
    
    struct LockData 
    {
        LockData(): mutex(0) {}
        LockData(ec2::QnDistributedMutex* mutex, QnVirtualCameraResourcePtr cameraResource): mutex(mutex), cameraResource(cameraResource) {}

        ec2::QnDistributedMutex* mutex;
        QnVirtualCameraResourcePtr cameraResource;
    };
    QMap<QString, LockData> m_lockInProgress;
    ec2::QnMutexCameraDataHandler* m_cameraDataHandler;
    QMutex m_mutex;

private:
    void addNewCamera(const QnVirtualCameraResourcePtr& cameraResource);
    void addNewCameraInternal(const QnVirtualCameraResourcePtr& cameraResource);

    //void requestFinished(const QnHTTPRawResponse& response, int handle);

    void at_mutexLocked();
    void at_mutexTimeout();
};

#endif //_server_appserver_processor_h_
