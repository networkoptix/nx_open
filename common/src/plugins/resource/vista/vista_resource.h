#ifndef vista_resource_h_1854
#define vista_resource_h_1854

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>

class QnVistaResource : public QnPlOnvifResource {
    Q_OBJECT
    typedef QnPlOnvifResource base_type;

public:
    QnVistaResource();
    virtual ~QnVistaResource();

    virtual int suggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps) const override;

protected:
    virtual QnAbstractPtzController *createPtzControllerInternal() override;
    virtual bool startInputPortMonitoringAsync( std::function<void(bool)>&& completionHandler ) override;
};

#endif //ENABLE_ONVIF

#endif //vista_resource_h_1854
