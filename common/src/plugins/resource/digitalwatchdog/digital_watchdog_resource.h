#ifndef dw_resource_h_1854
#define dw_resource_h_1854

#ifdef ENABLE_ONVIF

#include <core/datapacket/media_data_packet.h>
#include <core/resource/camera_resource.h>
#include <core/resource/security_cam_resource.h>

#include <plugins/resource/onvif/onvif_resource.h>

#include <utils/network/simple_http_client.h>

#include "dw_resource_settings.h"

class QnPlWatchDogResourceAdditionalSettings;
typedef QSharedPointer<QnPlWatchDogResourceAdditionalSettings> QnPlWatchDogResourceAdditionalSettingsPtr;

class QnDigitalWatchdogResource : public QnPlOnvifResource
{
    Q_OBJECT

    typedef QnPlOnvifResource base_type;

public:
    QnDigitalWatchdogResource();
    ~QnDigitalWatchdogResource();

    virtual int suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const override;

    virtual QnAbstractPtzController *createPtzControllerInternal() override;

    virtual bool setParamPhysical(const QString &id, const QString &value) override;
signals:
    void physicalParamChanged(const QString& name, const QString& value);
protected:
    virtual CameraDiagnostics::Result initInternal() override;
    virtual bool loadAdvancedParametersTemplate(QnCameraAdvancedParams &params) const override;
    virtual void initAdvancedParametersProviders(QnCameraAdvancedParams &params) override;
    virtual QSet<QString> calculateSupportedAdvancedParameters() const override;
    virtual void fetchAndSetAdvancedParameters() override;

    virtual bool loadImagingParams(QnCameraAdvancedParamValueMap &values) override;
private:
    bool isDualStreamingEnabled(bool& unauth);
    void enableOnvifSecondStream();
    QString fetchCameraModel();

private:
    bool m_hasZoom;
    QScopedPointer<DWCameraProxy> m_cameraProxy;

};

#endif //ENABLE_ONVIF

#endif //dw_resource_h_1854
