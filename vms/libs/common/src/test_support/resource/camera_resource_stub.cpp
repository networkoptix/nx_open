#include "camera_resource_stub.h"

#include <optional>

#include <core/resource/camera_user_attribute_pool.h>

namespace nx {

struct CameraResourceStub::Private
{
    mutable QnCameraUserAttributePool attributes;
    Qn::LicenseType licenseType = Qn::LicenseType::LC_Count; //< TODO: #GDM Replace with optional.
    std::optional<bool> hasDualStreaming;
};

CameraResourceStub::CameraResourceStub(Qn::LicenseType licenseType):
    d(new Private())
{
    d->licenseType = licenseType;
    setId(QnUuid::createUuid());
    addFlags(Qn::server_live_cam);
    setForceUseLocalProperties(true);
}

CameraResourceStub::~CameraResourceStub()
{
}

QnAbstractStreamDataProvider* CameraResourceStub::createLiveDataProvider()
{
    NX_ASSERT(false);
    return nullptr;
}

Qn::LicenseType CameraResourceStub::calculateLicenseType() const
{
    if (d->licenseType == Qn::LC_Count)
        return base_type::calculateLicenseType();

    return d->licenseType;
}

QnCameraUserAttributePool* CameraResourceStub::userAttributesPool() const
{
    return &d->attributes;
}

Qn::ResourceStatus CameraResourceStub::getStatus() const
{
    return Qn::Online;
}

bool CameraResourceStub::hasDualStreamingInternal() const
{
    if (d->hasDualStreaming.has_value())
        return *d->hasDualStreaming;

    return base_type::hasDualStreamingInternal();
}

void CameraResourceStub::setHasDualStreaming(bool value)
{
    d->hasDualStreaming = value;
    setProperty(ResourcePropertyKey::kHasDualStreaming, 1); //< to reset cached values;
}

void CameraResourceStub::markCameraAsNvr()
{
    setDeviceType(nx::core::resource::DeviceType::nvr);
}

void CameraResourceStub::setLicenseType(Qn::LicenseType licenseType)
{
    d->licenseType = licenseType;
    emit licenseTypeChanged(toSharedPointer(this));
}

bool CameraResourceStub::setProperty(const QString& key,
    const QString& value,
    PropertyOptions options)
{
    base_type::setProperty(key, value, options);
    emitPropertyChanged(key);
    return false;
}

bool CameraResourceStub::setProperty(
    const QString& key,
    const QVariant& value,
    PropertyOptions options)
{
    base_type::setProperty(key, value, options);
    emitPropertyChanged(key);
    return false;
}

void CameraResourceStub::markCameraAsVMax()
{
    d->licenseType = Qn::LC_VMAX;
    setProperty(ResourcePropertyKey::kDts, 1); //< to reset cached values;
    emit licenseTypeChanged(toSharedPointer(this));
}

} // namespace nx
