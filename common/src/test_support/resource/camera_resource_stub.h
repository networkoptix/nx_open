#pragma once

#include <boost/optional.hpp>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

namespace nx {

class CameraResourceStub: public QnVirtualCameraResource
{
    using base_type = QnVirtualCameraResource;
public:
    CameraResourceStub(Qn::LicenseType licenseType = Qn::LC_Professional);

    virtual QString getDriverName() const override;
    virtual Qn::ResourceStatus getStatus() const override;

    virtual bool hasDualStreaming() const override;
    void setHasDualStreaming(bool value);

    void markCameraAsNvr();
    void markCameraAsVMax();

protected:
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override;

    virtual Qn::LicenseType calculateLicenseType() const override;

private:
    Qn::LicenseType m_licenseType;
    boost::optional<bool> m_hasDualStreaming;
};

using CameraResourceStubPtr = QnSharedResourcePointer<CameraResourceStub>;

} // namespace nx
