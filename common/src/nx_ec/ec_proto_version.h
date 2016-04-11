/**********************************************************
* 6 nov 2014
* a.kolesnikov
***********************************************************/

#ifndef EC_PROTO_VERSION_H
#define EC_PROTO_VERSION_H

#include <utils/common/app_info.h>

namespace nx_ec
{
    //!EC2 protocol version
    /*!
        This version MUST be incremented by hand, when changes to transaction structures are done.
        Modules with different proto version do not connect to each other!
    */
    static const int EC2_PROTO_VERSION = QnAppInfo::ec2ProtoVersion();
    //!Servers with no proto version support have this version
    /*!
        THIS VALUE MUST NOT BE CHANGED!
    */
    static const int INITIAL_EC2_PROTO_VERSION = 1000;
}

#endif //EC_PROTO_VERSION_H
