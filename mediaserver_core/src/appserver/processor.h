#ifndef _server_appserver_processor_h_
#define _server_appserver_processor_h_

#include <nx_ec/ec_api.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_processor.h>

#include "api/app_server_connection.h"
#include "mutex/distributed_mutex.h"
#include <common/common_module_aware.h>

namespace ec2 {
    class QnMutexCameraDataHandler;
    class QnDistributedMutexManager;
    struct QnCameraUserAttributes;
}

class QnAppserverResourceProcessor:
    public QObject,
    public QnResourceProcessor,
    public QnCommonModuleAware
{
    Q_OBJECT

public:
    QnAppserverResourceProcessor(
        QnCommonModule* commonModule,
        ec2::QnDistributedMutexManager* distributedMutexManager,
        QnUuid serverId);
    virtual ~QnAppserverResourceProcessor();

    virtual bool isBusy() const override;
    virtual void processResources(const QnResourceList &resources) override;

    static ec2::ErrorCode addAndPropagateCamResource(
        QnCommonModule* commonModule,
        const ec2::ApiCameraData& apiCameraData,
        const ec2::ApiResourceParamDataList& properties
    );

private:
    ec2::AbstractECConnectionPtr m_ec2Connection;
    QnUuid m_serverId;

    struct LockData
    {
        LockData(): mutex(0) {}
        LockData(ec2::QnDistributedMutex* mutex, QnVirtualCameraResourcePtr cameraResource): mutex(mutex), cameraResource(cameraResource) {}

        ec2::QnDistributedMutex* mutex;
        QnVirtualCameraResourcePtr cameraResource;
    };
    QMap<QString, LockData> m_lockInProgress;
    ec2::QnMutexCameraDataHandler* m_cameraDataHandler = nullptr;
    QnMutex m_mutex;

private:
    void addNewCamera(const QnVirtualCameraResourcePtr& cameraResource);

    void addNewCameraInternal(const QnVirtualCameraResourcePtr& cameraResource) const;

    //void requestFinished(const QnHTTPRawResponse& response, int handle);

    void at_mutexLocked();
    void at_mutexTimeout();
    void readDefaultUserAttrs();
private:
    QnCameraUserAttributesPtr m_defaultUserAttrs;
    ec2::QnDistributedMutexManager* m_distributedMutexManager;
};

#endif //_server_appserver_processor_h_
