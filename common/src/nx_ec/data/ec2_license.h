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
        QString name;
        QByteArray key;
        qint32 cameraCount;
        QByteArray hardwareID;
        QByteArray signature;
        QnLicense::Type type;
        QString xclass;
        QString version;
        QString brand;
        //!Expiration time of this license, in milliseconds since epoch, or -1 if this license never expires
        qint64 expirationTime;
        QByteArray licenseBlock;

        ApiLicense();

        void fromResource( const QnLicense& lic );
        void toResource( QnLicense& license ) const;
    };

    #define ApiLicenseFields (name)(key)(cameraCount)(hardwareID)(signature)(type)(xclass)(version)(brand)(expirationTime)(licenseBlock)
    QN_DEFINE_STRUCT_SERIALIZATORS( ec2::ApiLicense, ApiLicenseFields )
}

namespace ec2
{
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
