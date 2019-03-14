#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/impl_ptr.h>

namespace nx {

class CameraResourceStub: public QnVirtualCameraResource
{
    using base_type = QnVirtualCameraResource;

public:
    CameraResourceStub(Qn::LicenseType licenseType = Qn::LC_Professional);
    virtual ~CameraResourceStub() override;

    virtual Qn::ResourceStatus getStatus() const override;

    virtual bool hasDualStreamingInternal() const override;
    void setHasDualStreaming(bool value);

    void markCameraAsNvr();
    void markCameraAsVMax();

    void setLicenseType(Qn::LicenseType licenseType);

    virtual bool setProperty(
        const QString& key,
        const QString& value,
        PropertyOptions options = DEFAULT_OPTIONS) override;

    virtual bool setProperty(
        const QString& key,
        const QVariant& value,
        PropertyOptions options = DEFAULT_OPTIONS) override;

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual Qn::LicenseType calculateLicenseType() const override;
    virtual QnCameraUserAttributePool* userAttributesPool() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

using CameraResourceStubPtr = QnSharedResourcePointer<CameraResourceStub>;

} // namespace nx
