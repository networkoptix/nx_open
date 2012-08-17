#ifndef onvif_resource_settings_h_2250
#define onvif_resource_settings_h_2250

#include "../camera_settings/camera_settings.h"
#include "soap_wrapper.h"

//
// OnvifCameraSettingsResp
//

class OnvifCameraSetting;
typedef QHash<QString, OnvifCameraSetting> CameraSettings;

class OnvifCameraSettingsResp
{
    ImagingSoapWrapper* m_rangesSoapWrapper;
    ImagingSoapWrapper* m_valsSoapWrapper;
    ImagingOptionsResp* m_rangesResponse;
    ImagingSettingsResp* m_valsResponse;
    const std::string m_videoSrcToken;
    const QString m_uniqId;
    CameraSettings m_cameraSettings;

public:

    OnvifCameraSettingsResp(const std::string& endpoint, const std::string& login, const std::string& passwd,
        const std::string& videoSrcToken, const QString& uniqId);

    ~OnvifCameraSettingsResp();

    bool isEmpty() const;
    bool makeGetRequest();
    bool makeSetRequest();
    const ImagingOptionsResp& getRangesResponse() const;
    const ImagingSettingsResp& getValsResponse() const;
    QString getEndpointUrl() const;
    QString getLogin() const;
    QString getPassword() const;
    QString getUniqueId() const;
    CameraSettings& getCameraSettings() { return m_cameraSettings; }

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

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const = 0;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const = 0;

protected:
    inline bool reinitSrc(OnvifCameraSettingsResp& src, bool reinit) const { return !reinit || src.makeGetRequest(); }
    template<class T> bool compareAndSendToCamera(T* field, const T val, OnvifCameraSettingsResp& src) const;
};

class OnvifCameraSettingOperationEmpty: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting&, OnvifCameraSettingsResp&, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting&, OnvifCameraSettingsResp&, bool reinitSrc = false) const;
};

class ImagingWhiteBalanceYbGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposureIrisOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposureExposureTimeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingWideDynamicRangeModeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingWhiteBalanceModeOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& AUTO_STR;
    static const QString& MANUAL_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposureMaxGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposureMinExposureTimeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingBacklightCompensationLevelOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingContrastOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposurePriorityOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& LOW_NOISE_STR;
    static const QString& FRAME_RATE_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingSharpnessOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingIrCutFilterModesOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& AUTO_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingWhiteBalanceYrGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposureMinGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposureGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposureModeOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& AUTO_STR;
    static const QString& MANUAL_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingWideDynamicRangeLevelOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposureMaxIrisOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingBrightnessOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposureMinIrisOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingBacklightCompensationModeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingColorSaturationOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class ImagingExposureMaxExposureTimeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class MaintenanceSystemRebootOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class MaintenanceSoftSystemFactoryDefaultOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
};

class MaintenanceHardSystemFactoryDefaultOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const;
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
        const QString& query,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue());

    virtual ~OnvifCameraSetting() {};

    OnvifCameraSetting& OnvifCameraSetting::operator=(const OnvifCameraSetting& rhs);

    bool getFromCamera(OnvifCameraSettingsResp& src) { return m_operation->get(*this, src); }
    bool setToCamera(OnvifCameraSettingsResp& src) { return m_operation->set(*this, src); }
};

//
// class OnvifCameraSettingReader
//

class OnvifCameraSettingReader: public CameraSettingReader
{
    static const QString& IMAGING_GROUP_NAME;
    static const QString& MAINTENANCE_GROUP_NAME;

    OnvifCameraSettingsResp& m_settings;

public:
    OnvifCameraSettingReader(OnvifCameraSettingsResp& onvifSettings, const QString& filepath);
    virtual ~OnvifCameraSettingReader();

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name);
    virtual bool isParamEnabled(const QString& id, const QString& parentId);
    virtual void paramFound(const CameraSetting& value, const QString& parentId);
    virtual void cleanDataOnFail();
    virtual void parentOfRootElemFound(const QString& parentId);

private:

    OnvifCameraSettingReader();
};

#endif //onvif_resource_settings_h_2250
