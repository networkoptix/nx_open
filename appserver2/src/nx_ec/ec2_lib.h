/**********************************************************
* 24 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_LIB_H
#define EC2_LIB_H

#include <nx_ec/ec_api.h>
#include <common/common_globals.h>


extern "C"
{
    /*!
        \return This object MUST be freed by caller using operator delete()
    */
    ec2::AbstractECConnectionFactory* getConnectionFactory( Qn::PeerType peerType );
}

#endif  //EC2_LIB_H
