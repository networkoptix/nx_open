#ifndef onvif_resource_settings_h_2250
#define onvif_resource_settings_h_2250

#include "../camera_settings/camera_settings.h"

struct OnvifCameraSettingsResp
{
    ImagingOptionsResp rangesResponse;
    ImagingSettingsResp valsResponse;

    OnvifCameraSettings()
    {
        rangesResponse.ImagingOptions = 0;
        valsResponse.ImagingSettings = 0;
    }

    bool isEmpty() const { return !rangesResponse.ImagingOptions || !valsResponse.ImagingSettings; }
};

class OnvifCameraSettingOperationAbstract
{
    static const QHash<QString, OnvifCameraSettingOperationAbstract*> operations;

public:

    virtual void get(CameraSetting& out, const OnvifCameraSettingsResp& src) = 0;
    virtual void set(CameraSetting& out, const OnvifCameraSettingsResp& src) = 0;
};

class OnvifCameraSetting: public CameraSetting
{
public:

    OnvifCameraSetting(const OnvifCameraSettingOperationAbstract& operation,
        const QString& id,
        const QString& name,
        WIDGET_TYPE type,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue());

private:
    OnvifCameraSetting();

protected:
    virtual ~OnvifCameraSetting() {};

private:
    const OnvifCameraSettingOperationAbstract& m_operation;
};

#endif //onvif_resource_settings_h_2250
