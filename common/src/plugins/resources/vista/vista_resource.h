#ifndef vista_resource_h_1854
#define vista_resource_h_1854

#ifdef ENABLE_ONVIF

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "../onvif/onvif_resource.h"

class QnVistaResource : public QnPlOnvifResource
{
    Q_OBJECT

    typedef QnPlOnvifResource base_type;

public:
    QnVistaResource();
    ~QnVistaResource();

    virtual int suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const override;
};
#endif //ENABLE_ONVIF

#endif //vista_resource_h_1854
