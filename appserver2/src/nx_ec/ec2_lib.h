#pragma once

#include <nx_ec/ec_api.h>
#include <common/common_globals.h>


namespace nx {
namespace utils {

class TimerManager;

} // namespace utils
} // namespace nx

class QnCommonModule;

extern "C"
{
    /*!
        \return This object MUST be freed by caller using operator delete()
    */
    ec2::AbstractECConnectionFactory* getConnectionFactory(
        Qn::PeerType peerType,
        nx::utils::TimerManager* const timerManager,
        QnCommonModule* commonModule,
        bool isP2pMode);
}

