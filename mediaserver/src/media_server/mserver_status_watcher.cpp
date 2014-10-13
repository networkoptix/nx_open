/**********************************************************
* 22 sep 2014
* a.kolesnikov
***********************************************************/

#include "mserver_status_watcher.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/common/synctime.h>

#include "business/business_event_connector.h"
#include "serverutil.h"


static const long long USEC_PER_MSEC = 1000;

MediaServerStatusWatcher::MediaServerStatusWatcher()
{
    connect(qnResPool, &QnResourcePool::statusChanged, this, &MediaServerStatusWatcher::at_resource_statusChanged);
}

MediaServerStatusWatcher::~MediaServerStatusWatcher()
{
}

void MediaServerStatusWatcher::at_resource_statusChanged( const QnResourcePtr& resource )
{
    const QnMediaServerResourcePtr& mserverRes = resource.dynamicCast<QnMediaServerResource>();
    if( !mserverRes )
        return;

    if( mserverRes->getStatus() != Qn::ResourceStatus::Offline )
        return;

    //deciding, if it is we who is expected to generate this event
        //next (in guid ascending order) online server after fallen one is expected to generate this event
        //it is possible, that multiple servers will decide to generate event, if servers statuses are being changed at the moment, but this is OK for now

    const QnMediaServerResourceList& mserversList = qnResPool->getResources<QnMediaServerResource>();
    if( !mserversList.isEmpty() )   //in a strange case when there are no servers generating event
    {
        std::map<QnUuid, QnMediaServerResource*> mserversById;
        for( const QnMediaServerResourcePtr& mserver: mserversList )
            mserversById.emplace( mserver->getId(), mserver.data() );

        auto nextAfterFallenOneIter = mserversById.upper_bound( mserverRes->getId() );
        //skipping offline servers
        for( int i = 0; i < mserversById.size(); ++i )
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

    qnBusinessRuleConnector->at_mserverFailure(
        mserverRes,
        qnSyncTime->currentMSecsSinceEpoch() * USEC_PER_MSEC,
        QnBusiness::ServerTerminatedReason,
        QString() );
}
