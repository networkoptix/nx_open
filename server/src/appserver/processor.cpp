#include "processor.h"

#include "core/resourcemanagment/resource_pool.h"

QnAppserverResourceProcessor::QnAppserverResourceProcessor(const QnId& serverId, QnResourceFactory& resourceFactory)
    : m_serverId(serverId)
{
    m_appServer = QnAppServerConnectionFactory::createConnection(resourceFactory);
}

void QnAppserverResourceProcessor::processResources(const QnResourceList &resources)
{
    QnResourceList cameras;

    foreach (QnResourcePtr resource, resources)
    {
        QnNetworkResourcePtr networkResource = resource.dynamicCast<QnNetworkResource>();

        if (networkResource.isNull())
            continue;

        QnSecurityCamResourcePtr securityCamResource = resource.dynamicCast<QnSecurityCamResource>();

        if (!securityCamResource.isNull())
        {
            QnScheduleTaskList taskList;
            taskList.append(QnScheduleTask("", "", 11, 12,true, QnScheduleTask::RecordingType_MotionOnly, 1, 2, 3, QnQualityHighest, 7));
            taskList.append(QnScheduleTask("", "", 21, 22,true, QnScheduleTask::RecordingType_Never, 5, 6, 7, QnQualityLow, 5));
            securityCamResource->setScheduleTasks(taskList);
        }

        m_appServer->addCamera(*networkResource, m_serverId, cameras);
    }

    QnResourcePool::instance()->addResources(cameras);
}
