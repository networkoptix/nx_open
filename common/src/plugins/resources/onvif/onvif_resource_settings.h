#ifndef onvif_resource_settings_h_2250
#define onvif_resource_settings_h_2250

#include "../camera_settings/camera_settings.h"
#include "soap_wrapper.h"

//
// OnvifCameraSettingsResp
//

class OnvifCameraSettingsResp
{
    ImagingSoapWrapper* m_rangesSoapWrapper;
    ImagingSoapWrapper* m_valsSoapWrapper;
    ImagingOptionsResp* m_rangesResponse;
    ImagingSettingsResp* m_valsResponse;
    const std::string m_videoSrcToken;
    const QString m_uniqId;

public:

    OnvifCameraSettingsResp(const std::string& endpoint, const std::string& login, const std::string& passwd,
        const std::string& videoSrcToken, const QString& uniqId);

    ~OnvifCameraSettingsResp();

    bool isEmpty() const;
    bool makeRequest();
    const ImagingOptionsResp& getRangesResponse() const;
    const ImagingSettingsResp& getValsResponse() const;

private:

    OnvifCameraSettingsResp();
};

//
// OnvifCameraSettingOperation
//

class CommonStringValues
{
public:

    static const QString& ON;
    static const QString& OFF;
    static const QString& SEPARATOR;
};

class OnvifCameraSettingOperationAbstract
{
public:

    static const OnvifCameraSettingOperationAbstract& EMPTY_OPERATION;

    static const QHash<QString, OnvifCameraSettingOperationAbstract*> operations;

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) = 0;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) = 0;

protected:
    inline bool reinitSrc(OnvifCameraSettingsResp& src, bool reinit) const { return !reinit || src.makeRequest(); }
};

class OnvifCameraSettingOperationEmpty: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting&, OnvifCameraSettingsResp&, bool reinitSrc = false) { return true; };
    virtual bool set(const CameraSetting&, OnvifCameraSettingsResp&, bool reinitSrc = false) { return true; };
};

class ImagingWhiteBalanceYbGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposureIrisOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposureExposureTimeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingWideDynamicRangeModeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingWhiteBalanceModeOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& AUTO_STR;
    static const QString& MANUAL_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposureMaxGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposureMinExposureTimeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingBacklightCompensationLevelOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingContrastOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposurePriorityOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& LOW_NOISE_STR;
    static const QString& FRAME_RATE_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingSharpnessOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingIrCutFilterModesOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& AUTO_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingWhiteBalanceYrGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposureMinGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposureGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposureModeOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& AUTO_STR;
    static const QString& MANUAL_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingWideDynamicRangeLevelOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposureMaxIrisOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingBrightnessOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposureMinIrisOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingBacklightCompensationModeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingColorSaturationOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class ImagingExposureMaxExposureTimeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class MaintenanceSystemRebootOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class MaintenanceSoftSystemFactoryDefaultOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};

class MaintenanceHardSystemFactoryDefaultOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false);
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false);
};


//
// OnvifCameraSetting
//

class OnvifCameraSetting: public CameraSetting
{
    const OnvifCameraSettingOperationAbstract* m_operation;

public:

    OnvifCameraSetting(): CameraSetting(), m_operation(&OnvifCameraSettingOperationAbstract::EMPTY_OPERATION) {};

    OnvifCameraSetting(const OnvifCameraSettingOperationAbstract& operation,
        const QString& id,
        const QString& name,
        WIDGET_TYPE type,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue());

    virtual ~OnvifCameraSetting() {};

    OnvifCameraSetting& OnvifCameraSetting::operator=(const OnvifCameraSetting& rhs);
};

#endif //onvif_resource_settings_h_2250
