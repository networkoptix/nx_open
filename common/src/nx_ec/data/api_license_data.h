/**********************************************************
* 4 mar 2014
* a.kolesnikov
***********************************************************/

#ifndef API_LICENSE_DATA_H
#define API_LICENSE_DATA_H

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

#endif  //API_LICENSE_DATA_H
