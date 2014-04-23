/**********************************************************
* 4 mar 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_LICENSE_H
#define EC2_LICENSE_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiLicenseData : ApiData {
        QByteArray key;
        QByteArray licenseBlock;
    };
#define ApiLicenseData_Fields (key)(licenseBlock)


/*    struct ApiLicense : ApiLicenseData
    {
    public:
        void fromResource( const QnLicense& lic );
        void toResource( QnLicense& license ) const;
    };


    struct ApiLicenseList : std::vector<ApiLicense>
    {
    public:
        void fromResourceList( const QnLicenseList& licList );
        void toResourceList( QnLicenseList& licenseList ) const;
    };*/
}

#endif  //EC2_LICENSE_H
