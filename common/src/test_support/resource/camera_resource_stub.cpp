#include "camera_resource_stub.h"

namespace nx {

    CameraResourceStub::CameraResourceStub(Qn::LicenseType licenseType):
        m_licenseType(licenseType)
{
    setId(QnUuid::createUuid());
    addFlags(Qn::server_live_cam);
}

QString CameraResourceStub::getDriverName() const
{
    return lit("CameraResourceStub");
}

QnAbstractStreamDataProvider * CameraResourceStub::createLiveDataProvider()
{
    NX_ASSERT(false);
    return nullptr;
}

Qn::ResourceStatus CameraResourceStub::getStatus() const
{
    return Qn::Online;
}

Qn::LicenseType CameraResourceStub::licenseType() const
{
    return m_licenseType;
}

} // namespace nx
