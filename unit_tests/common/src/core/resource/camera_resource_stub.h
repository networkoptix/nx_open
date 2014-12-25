#ifndef QN_CAMERA_RESOURCE_STUB_H
#define QN_CAMERA_RESOURCE_STUB_H

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>

class QnCameraResourceStub: public QnVirtualCameraResource {
public:
    QnCameraResourceStub();

    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override;
    virtual Qn::ResourceStatus getStatus() const override;


protected:
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override;
};


#endif // QN_CAMERA_RESOURCE_STUB_H