/**********************************************************
* 24 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_lib.h"

#include <ec2/local_connection_factory.h>
#include <ec2/remote_connection_factory.h>

extern "C"
{
    ec2::RemoteConnectionFactory* getRemoteConnectionFactory(
        Qn::PeerType peerType,
        nx::utils::TimerManager* const timerManager,
        QnCommonModule* commonModule,
        bool isP2pMode)
    {
		return new ec2::RemoteConnectionFactory(peerType, timerManager, commonModule, isP2pMode);
    }

	ec2::LocalConnectionFactory* getLocalConnectionFactory(
		Qn::PeerType peerType,
		nx::utils::TimerManager* const timerManager,
		QnCommonModule* commonModule,
		bool isP2pMode)
	{
		return new ec2::LocalConnectionFactory(peerType, timerManager, commonModule, isP2pMode);
	}

}
