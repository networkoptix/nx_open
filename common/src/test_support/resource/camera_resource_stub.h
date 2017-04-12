#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

namespace nx {

class CameraResourceStub: public QnVirtualCameraResource
{
public:
    CameraResourceStub(Qn::LicenseType licenseType = Qn::LC_Professional);

    virtual QString getDriverName() const override;
    virtual Qn::ResourceStatus getStatus() const override;

    virtual Qn::LicenseType licenseType() const override;

protected:
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override;

private:
    Qn::LicenseType m_licenseType;
};

} // namespace nx
