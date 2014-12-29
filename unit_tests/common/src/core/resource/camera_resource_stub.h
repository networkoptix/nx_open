#ifndef QN_CAMERA_RESOURCE_STUB_H
#define QN_CAMERA_RESOURCE_STUB_H

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

class QnCameraResourceStub: public QnVirtualCameraResource {
public:
    QnCameraResourceStub(Qn::LicenseType cameraType = Qn::LC_Professional);

    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override;
    virtual Qn::ResourceStatus getStatus() const override;

    virtual Qn::LicenseType licenseType() const override;
protected:
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override;

private:
    Qn::LicenseType m_cameraType;
};


#endif // QN_CAMERA_RESOURCE_STUB_H