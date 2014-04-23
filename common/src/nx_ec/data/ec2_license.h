/**********************************************************
* 4 mar 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_LICENSE_H
#define EC2_LICENSE_H

#include <vector>

#include "api_data.h"
#include "licensing/license.h"
#include "ec2_license_i.h"

namespace ec2
{
    struct ApiLicense : ApiLicenseData
    {
    public:
        void fromResource( const QnLicense& lic );
        void toResource( QnLicense& license ) const;
    };
	
    struct ApiLicenseList : ApiLicenseListData
    {
    public:
        void fromResourceList( const QnLicenseList& licList );
        void toResourceList( QnLicenseList& licenseList ) const;
    };
}

#endif  //EC2_LICENSE_H
