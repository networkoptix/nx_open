/**********************************************************
* 4 mar 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_LICENSE_H
#define EC2_LICENSE_H

#include <vector>

#include "api_data.h"
#include "licensing/license.h"


namespace ec2
{
    class ApiLicense
    :
        public ApiData
    {
    public:
        QByteArray key;
        QByteArray licenseBlock;

        ApiLicense();

        void fromResource( const QnLicense& lic );
        void toResource( QnLicense& license ) const;
    };

	
	#define ApiLicenseFields (key)(licenseBlock)
	QN_DEFINE_STRUCT_SERIALIZATORS( ApiLicense, ApiLicenseFields )
	
    class ApiLicenseList
    :
        public ApiData
    {
    public:
        std::vector<ApiLicense> data;

        void fromResourceList( const QnLicenseList& licList );
        void toResourceList( QnLicenseList& licenseList ) const;
    };

	QN_DEFINE_STRUCT_SERIALIZATORS( ApiLicenseList, (data) )
}

#endif  //EC2_LICENSE_H
