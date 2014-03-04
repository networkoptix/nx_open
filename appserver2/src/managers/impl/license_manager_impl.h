/**********************************************************
* 4 mar 2014
* akolesnikov
***********************************************************/

#ifndef LICENSE_MANAGER_IMPL_H
#define LICENSE_MANAGER_IMPL_H

#include <nx_ec/data/ec2_license.h>

#include "transaction/transaction.h"


namespace ec2
{
    //!Contains license-related logic. For db operations uses corresponding QnDbManager methods
    /*!
        \note All methods are synchronous
        \note All methods are re-enterable
    */
    class LicenseManagerImpl
    {
    public:
        LicenseManagerImpl();

        ErrorCode getLicenses( ApiLicenseList* const licList );
        ErrorCode addLicenses( const ApiLicenseList& licenses );

        void getHardwareId( ServerInfo* const serverInfo );
    };
}

#endif  //LICENSE_MANAGER_IMPL_H
