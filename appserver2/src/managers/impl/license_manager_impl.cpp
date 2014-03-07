/**********************************************************
* 4 mar 2014
* akolesnikov
***********************************************************/

#include "license_manager_impl.h"
#include "llutil/hardware_id.h"


static const char TEST_LICENSE_NX[] = 
    "NAME=hdwitness\n"
    "SERIAL=5A7B-EYJO-914U-7MKD\n"
    "HWID=0333333333333333333333333333333333\n"
    "COUNT=100\n"
    "CLASS=digital\n"
    "VERSION=20\n"
    "BRAND=hdwitness\n"
    "EXPIRATION=\n"
    "SIGNATURE=Tb7NULLsgJb6cj0cZdYLGC04Y/i2etfCQqUZbL4I6hXstWfCnoV5qQPoDvTY8gpVGR9RfPLeHLiDzbuFW3p1ZQ==\n"
    "SIGNATURE2=n2uJGJ9f/La1GgO7SHkxdTrErcs0NvK7PbYbdkUmJWAUXItKCudMNi6YPQMqG4aCc2F5XnfdWNGAPLlELVjnng==\n";

static const char TEST_LICENSE_DW[] = 
    "NAME=dwspectrum\n"
    "SERIAL=EWXZ-OXEW-NTRP-T8RP\n"
    "HWID=0333333333333333333333333333333333\n"
    "COUNT=100\n"
    "CLASS=digital\n"
    "VERSION=20\n"
    "BRAND=dwspectrum\n"
    "EXPIRATION=\n"
    "SIGNATURE=N5hqvo0eBbPcuCEGVQJ29XTYZalkpThlZTcTkSLvqKSGWfbv2KCIV/0ntOhRM+JiX0oysOhM81/2oTtK37VlDw==\n"
    "SIGNATURE2=CrZsJh2xzOhKsMRgFA9AztwYrDLed6KgSLxdbGjEQ5cpqL0m8GxE9A/jNWbHP9MGWynfHsFGVWTeMCtnRkDIlg==\n";


namespace ec2
{
    LicenseManagerImpl::LicenseManagerImpl()
    {
    }

    ErrorCode LicenseManagerImpl::getLicenses( ApiLicenseList* const licList )
    {
        //TODO/IMPL

        ApiLicense license;

        license.licenseBlock = TEST_LICENSE_NX;
        licList->data.push_back( license );

        license.licenseBlock = TEST_LICENSE_DW;
        licList->data.push_back( license );

        return ErrorCode::ok;
    }

    ErrorCode LicenseManagerImpl::addLicenses( const ApiLicenseList& licenses )
    {
        //TODO/IMPL
        return ErrorCode::notImplemented;
    }

    void LicenseManagerImpl::getHardwareId( ServerInfo* const serverInfo )
    {
        int guidCompatibility = 0;

        // TODO: #Ivan, add guidCompatibility to settings
        serverInfo->mainHardwareIds = LLUtil::getMainHardwareIds(guidCompatibility);
        serverInfo->compatibleHardwareIds = LLUtil::getCompatibleHardwareIds(guidCompatibility);
    }
}
