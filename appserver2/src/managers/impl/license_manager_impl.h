/**********************************************************
* 4 mar 2014
* akolesnikov
***********************************************************/

#ifndef LICENSE_MANAGER_IMPL_H
#define LICENSE_MANAGER_IMPL_H

#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtCore/QString>

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
        bool validateLicense(const ApiLicense& license) const;

    private:
        QList<QByteArray> m_hardwareIds;
        QString m_brand;
    };
}

#endif  //LICENSE_MANAGER_IMPL_H
