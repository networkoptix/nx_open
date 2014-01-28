/**********************************************************
* 24 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_LIB_H
#define EC2_LIB_H

#include <nx_ec/ec_api.h>


extern "C"
{
    /*!
        \return This object MUST be freed by caller using operator delete()
    */
    EC2_LIB_API ec2::AbstractECConnectionFactory* getConnectionFactory();
}

#endif  //EC2_LIB_H
