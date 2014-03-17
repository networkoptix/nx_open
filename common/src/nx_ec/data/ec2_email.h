#ifndef __EC2__EMAIL_DATA_H_
#define __EC2__EMAIL_DATA_H_

#include <vector>

#include "api_data.h"
#include "utils/common/email.h"


namespace ec2
{
    class ApiEmailSettingsData
    :
        public ApiData
    {
    public:
        QString host;
        int port;
        QString user;
        QString password;
        QnEmail::ConnectionType connectionType;

        ApiEmailSettingsData();

        void fromResource( const QnEmail::Settings& settings );
        void toResource( QnEmail::Settings& settings ) const;
    };

	#define ApiEmailSettingsFields (host)(port)(user)(password)(connectionType)
	QN_DEFINE_STRUCT_SERIALIZATORS( ApiEmailSettingsData, ApiEmailSettingsFields )
	
    /* class ApiEmailSettingsData
    :
        public ApiData
    {
    };

    class ApiLicenseList
    :
        public ApiData
    {
    public:
        std::vector<ApiLicense> data;

        void fromResourceList( const QnLicenseList& licList );
        void toResourceList( QnLicenseList& licenseList ) const;
    };

	QN_DEFINE_STRUCT_SERIALIZATORS( ApiLicenseList, (data) ) */
}

#endif // __EC2__EMAIL_DATA_H_