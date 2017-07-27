/**********************************************************
* 24 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_lib.h"

#include "connection_factory.h"

extern "C"
{
    ec2::AbstractECConnectionFactory* getConnectionFactory(
        Qn::PeerType peerType,
        nx::utils::TimerManager* const timerManager,
        QnCommonModule* commonModule,
        bool isP2pMode)
    {
        return new ec2::Ec2DirectConnectionFactory(peerType, timerManager, commonModule, isP2pMode);
    }
}
