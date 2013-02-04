#ifndef dw_resource_h_1854
#define dw_resource_h_1854

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"
#include "../onvif/onvif_resource.h"
#include "dw_resource_settings.h"

class QnPlWatchDogResourceAdditionalSettings;
typedef QSharedPointer<QnPlWatchDogResourceAdditionalSettings> QnPlWatchDogResourceAdditionalSettingsPtr;

class QnPlWatchDogResource : public QnPlOnvifResource
{
    Q_OBJECT

    typedef QnPlOnvifResource base_type;

public:
    QnPlWatchDogResource();
    ~QnPlWatchDogResource();

    virtual int suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const override;

    virtual QnAbstractPtzController *getPtzController() override;

protected:
    bool initInternal() override;
    virtual void fetchAndSetCameraSettings() override;
    virtual bool getParamPhysical(const QnParam &param, QVariant &val) override;
    virtual bool setParamPhysical(const QnParam &param, const QVariant& val) override;

private:
    bool isDualStreamingEnabled(bool& unauth);
    void enableOnvifSecondStream();
    QString fetchCameraModel();

private:
    friend class QnWatchDogPtzController; // TODO: remove

    QScopedPointer<QnAbstractPtzController> m_ptzController;

    //The List contains hierarchy of DW models from child to parent "DIGITALWATCHDOG" (see in camera_settings.xml)
    //The grandparent "ONVIF" is processed by invoking of parent 'fetchAndSetCameraSettings' method
    typedef QList<QnPlWatchDogResourceAdditionalSettingsPtr> AdditionalSettings;
    AdditionalSettings m_additionalSettings;
};


class QnPlWatchDogResourceAdditionalSettings
{
    QnPlWatchDogResourceAdditionalSettings();

public:
    
    QnPlWatchDogResourceAdditionalSettings(const QString& host, int port, unsigned int timeout,
        const QAuthenticator& auth, const QString& cameraSettingId);
    ~QnPlWatchDogResourceAdditionalSettings();

    bool refreshValsFromCamera();
    bool getParamPhysicalFromBuffer(const QnParam &param, QVariant &val);
    bool setParamPhysical(const QnParam &param, const QVariant& val);

private:
    DWCameraProxy* m_cameraProxy;
    DWCameraSettings m_settings;
};

#endif //dw_resource_h_1854
