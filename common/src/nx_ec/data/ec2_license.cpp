/**********************************************************
* 4 mar 2014
* a.kolesnikov
***********************************************************/

#include "ec2_license.h"


namespace ec2
{
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
        data.clear();
        data.resize( licList.size() );
        int i = 0;
        for( const QnLicensePtr& lic: licList )
            data[i++].fromResource( *lic );
    }

    void ApiLicenseList::toResourceList( QnLicenseList& licenseList ) const
    {
        licenseList.reserve(licenseList.size() + data.size());
        for( int i = 0; i < data.size(); ++i )
        {
            QnLicensePtr license(new QnLicense());
            data[i].toResource(*license.data());
            licenseList.push_back( license );
        }
    }
}
