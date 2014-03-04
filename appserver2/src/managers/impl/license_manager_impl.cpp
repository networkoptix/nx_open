/**********************************************************
* 4 mar 2014
* akolesnikov
***********************************************************/

#include "license_manager_impl.h"


static const char TEST_LICENSE[] = 
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

namespace ec2
{
    LicenseManagerImpl::LicenseManagerImpl()
    {
    }

    ErrorCode LicenseManagerImpl::getLicenses( ApiLicenseList* const licList )
    {
        //TODO/IMPL

        ApiLicense license;
        license.licenseBlock = TEST_LICENSE;

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
        serverInfo->oldHardwareId = "0333333333333333333333333333333333";
        serverInfo->hardwareId1 = "0333333333333333333333333333333333";
        serverInfo->hardwareId2 = "0333333333333333333333333333333333";
        serverInfo->hardwareId3 = "0333333333333333333333333333333333";
    }
}
