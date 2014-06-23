/**********************************************************
* 4 mar 2014
* akolesnikov
***********************************************************/

#ifndef LICENSE_MANAGER_IMPL_H
#define LICENSE_MANAGER_IMPL_H

#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx_ec/data/api_license_data.h>

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

        ErrorCode getLicenses( ApiLicenseDataList* const licList );
        ErrorCode addLicenses( const ApiLicenseDataList& licenses );

        void getHardwareId( ec2::ApiRuntimeData* const serverInfo );
        bool validateLicense(const ApiLicenseData& license) const;
    };
}

#endif  //LICENSE_MANAGER_IMPL_H
