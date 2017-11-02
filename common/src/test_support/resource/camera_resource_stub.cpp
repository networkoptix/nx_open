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

Qn::LicenseType CameraResourceStub::calculateLicenseType() const
{
    return m_licenseType;
}

Qn::ResourceStatus CameraResourceStub::getStatus() const
{
    return Qn::Online;
}

bool CameraResourceStub::hasDualStreaming() const
{
    if (m_hasDualStreaming.is_initialized())
        return m_hasDualStreaming.value();

    return base_type::hasDualStreaming();
}

void CameraResourceStub::setHasDualStreaming(bool value)
{
    m_hasDualStreaming = value;
    setProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME, 1); //< to reset cached values;
}

} // namespace nx
