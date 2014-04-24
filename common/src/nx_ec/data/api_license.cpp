
#if 0
/**********************************************************
* 4 mar 2014
* a.kolesnikov
***********************************************************/

#include "ec2_license.h"

namespace ec2
{

    //QN_DEFINE_STRUCT_SERIALIZATORS(ApiLicenseData, ApiLicenseFields)
    QN_FUSION_DECLARE_FUNCTIONS(ApiLicenseData, (binary))

        //QN_DEFINE_API_OBJECT_LIST_DATA(ApiLicense)


    void ApiLicense::fromResource( const QnLicense& lic )
    {
        key = lic.key();
        licenseBlock = lic.rawLicense();
    }

    void ApiLicense::toResource( QnLicense& license ) const
    {
        license.loadLicenseBlock( licenseBlock );
    }


    void ApiLicenseList::fromResourceList( const QnLicenseList& licList )
    {
        clear();
        resize( licList.size() );
        int i = 0;
        for( const QnLicensePtr& lic: licList )
            (*this)[i++].fromResource( *lic );
    }

    void ApiLicenseList::toResourceList( QnLicenseList& licenseList ) const
    {
        licenseList.reserve(licenseList.size() + size());
        for( int i = 0; i < size(); ++i )
        {
            QnLicensePtr license(new QnLicense());
            (*this)[i].toResource(*license.data());
            licenseList.push_back( license );
        }
    }
}
#endif