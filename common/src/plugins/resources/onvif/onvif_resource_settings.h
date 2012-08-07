#ifndef onvif_resource_settings_h_2250
#define onvif_resource_settings_h_2250

#include "../camera_settings/camera_settings.h"
#include "soap_wrapper.h"

class OnvifCameraSettingsResp
{
    ImagingBindingProxy m_rangesSoapWrapper;
    ImagingBindingProxy m_valsSoapWrapper;
    ImagingOptionsResp m_rangesResponse;
    ImagingSettingsResp m_valsResponse;
    const std::string m_videoSrcToken;
    const QString m_uniqId;

public:

    OnvifCameraSettings(const std::string& endpoint, const std::string& login, const std::string& passwd,
        const std::string& videoSrcToken, const QString& uniqId);

    bool isEmpty() const;
    bool makeRequest(const QString& uniqueId);
    const ImagingOptionsResp& getRangesREsponse() const;
    const ImagingSettingsResp& getValsREsponse() const;

private:

    OnvifCameraSettingsResp();
};

class OnvifCameraSettingOperationAbstract
{
public:

    static const QHash<QString, OnvifCameraSettingOperationAbstract*> operations;

    virtual void get(CameraSetting& out, OnvifCameraSettingsResp& src, bool reinitSrc = false) = 0;
    virtual void set(CameraSetting& out, OnvifCameraSettingsResp& src, bool reinitSrc = false) = 0;
};

class OnvifCameraSettingOperationEmpty: public OnvifCameraSettingOperationAbstract
{
public:
    virtual void get(CameraSetting& out, OnvifCameraSettingsResp& src, bool reinitSrc = false) {};
    virtual void set(CameraSetting& out, OnvifCameraSettingsResp& src, bool reinitSrc = false) {};
};

OnvifCameraSettingOperationEmpty EMPTY_OPERATION;

class OnvifCameraSetting: public CameraSetting
{
public:

    OnvifCameraSetting(): CameraSetting(), m_operation(EMPTY_OPERATION) {};

    OnvifCameraSetting(const OnvifCameraSettingOperationAbstract& operation,
        const QString& id,
        const QString& name,
        WIDGET_TYPE type,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue());

protected:
    virtual ~OnvifCameraSetting() {};

private:
    const OnvifCameraSettingOperationAbstract& m_operation;
};

#endif //onvif_resource_settings_h_2250
