/**********************************************************
* 22 sep 2014
* a.kolesnikov
***********************************************************/

#include "mserver_status_watcher.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <media_server/serverutil.h>
#include <utils/common/synctime.h>
#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/server_failure_event.h>
#include <nx/mediaserver/event/event_connector.h>
#include <nx/mediaserver/event/rule_processor.h>

static const long long USEC_PER_MSEC = 1000;
static const int SEND_ERROR_TIMEOUT = 1000 * 60;

MediaServerStatusWatcher::MediaServerStatusWatcher(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connect(commonModule->resourcePool(), &QnResourcePool::statusChanged, this, &MediaServerStatusWatcher::at_resource_statusChanged);
    connect(commonModule->resourcePool(), &QnResourcePool::resourceRemoved, this, &MediaServerStatusWatcher::at_resource_removed);
}

MediaServerStatusWatcher::~MediaServerStatusWatcher()
{
}

void MediaServerStatusWatcher::sendError()
{
    auto itr = m_candidatesToError.begin();
    while (itr != m_candidatesToError.end()) {
        OfflineServerData& data = itr.value();
        if (data.timer.elapsed() > SEND_ERROR_TIMEOUT/2)
        {
            qnEventRuleProcessor->processEvent(data.serverData);
            itr = m_candidatesToError.erase(itr);
        }
        else {
            ++itr;
        }
    }
}

void MediaServerStatusWatcher::at_resource_removed( const QnResourcePtr& resource )
{
    m_candidatesToError.remove(resource->getId());
    m_onlineServers.remove(resource->getId());
}

void MediaServerStatusWatcher::at_resource_statusChanged( const QnResourcePtr& resource )
{

    const QnMediaServerResourcePtr& mserverRes = resource.dynamicCast<QnMediaServerResource>();
    if( !mserverRes )
        return;

    if( mserverRes->getStatus() != Qn::ResourceStatus::Offline ) {
        m_onlineServers << mserverRes->getId();
        m_candidatesToError.remove(mserverRes->getId());
        return;
    }

    if (!m_onlineServers.contains(mserverRes->getId()))
        return; // we interesting in online->offline changes only
    m_onlineServers.remove(mserverRes->getId());

    //deciding, if it is we who is expected to generate this event
        //next (in guid ascending order) online server after fallen one is expected to generate this event
        //it is possible, that multiple servers will decide to generate event, if servers statuses are being changed at the moment, but this is OK for now

    const QnMediaServerResourceList& mserversList = resourcePool()->getResources<QnMediaServerResource>();
    if( !mserversList.isEmpty() )   //in a strange case when there are no servers generating event
    {
        std::map<QnUuid, QnMediaServerResource*> mserversById;
        for( const QnMediaServerResourcePtr& mserver: mserversList )
            mserversById.emplace( mserver->getId(), mserver.data() );

        auto nextAfterFallenOneIter = mserversById.upper_bound( mserverRes->getId() );
        //skipping offline servers
        for( size_t i = 0; i < mserversById.size(); ++i )
        {
            if( nextAfterFallenOneIter == mserversById.end() )
                nextAfterFallenOneIter = mserversById.begin();
            if( nextAfterFallenOneIter->second->getStatus() == Qn::Online ||
                nextAfterFallenOneIter->first == serverGuid() )     //we are online, whatever resource pool says
            {
                break;
            }
            ++nextAfterFallenOneIter;
        }

        if( nextAfterFallenOneIter == mserversById.cend() || nextAfterFallenOneIter->first != serverGuid() )
            return; //it is not we who was chosen to send event
    }

    nx::vms::event::ServerFailureEventPtr event(new class nx::vms::event::ServerFailureEvent(
        mserverRes, qnSyncTime->currentMSecsSinceEpoch() * USEC_PER_MSEC,
        nx::vms::event::ServerTerminatedReason, QString()));
    OfflineServerData data;
    data.serverData = event;
    m_candidatesToError[mserverRes->getId()] = data;
    QTimer::singleShot(SEND_ERROR_TIMEOUT, this, SLOT(sendError()));
    /*
    qnEventRuleConnector->at_mserverFailure(
        mserverRes,
        qnSyncTime->currentMSecsSinceEpoch() * USEC_PER_MSEC,
        nx::vms::event::ServerTerminatedReason,
        QString() );
    */
}
