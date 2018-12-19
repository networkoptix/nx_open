#include "camera_resource_stub.h"

namespace nx {

CameraResourceStub::CameraResourceStub(Qn::LicenseType licenseType):
    m_licenseType(licenseType)
{
    setId(QnUuid::createUuid());
    addFlags(Qn::server_live_cam);
}

QnAbstractStreamDataProvider* CameraResourceStub::createLiveDataProvider()
{
    NX_ASSERT(false);
    return nullptr;
}

Qn::LicenseType CameraResourceStub::calculateLicenseType() const
{
    if (m_licenseType == Qn::LC_Count)
        return base_type::calculateLicenseType();
    return m_licenseType;
}

Qn::ResourceStatus CameraResourceStub::getStatus() const
{
    return Qn::Online;
}

bool CameraResourceStub::hasDualStreamingInternal() const
{
    if (m_hasDualStreaming.is_initialized())
        return m_hasDualStreaming.value();

    return base_type::hasDualStreamingInternal();
}

void CameraResourceStub::setHasDualStreaming(bool value)
{
    m_hasDualStreaming = value;
    setProperty(ResourcePropertyKey::kHasDualStreaming, 1); //< to reset cached values;
}

void CameraResourceStub::markCameraAsNvr()
{
    setDeviceType(nx::core::resource::DeviceType::nvr);
}

void CameraResourceStub::setLicenseType(Qn::LicenseType licenseType)
{
    m_licenseType = licenseType;
    emit licenseTypeChanged(toSharedPointer(this));
}

void CameraResourceStub::markCameraAsVMax()
{
    m_licenseType = Qn::LC_VMAX;
    setProperty(ResourcePropertyKey::kDts, 1); //< to reset cached values;
    emit licenseTypeChanged(toSharedPointer(this));
}

} // namespace nx
