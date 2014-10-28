#ifndef onvif_resource_settings_h_2250
#define onvif_resource_settings_h_2250

#ifdef ENABLE_ONVIF

#include "../camera_settings/camera_settings.h"
#include "soap_wrapper.h"

#include <utils/camera/camera_diagnostics.h>


//
// OnvifCameraSettingsResp
//

class OnvifCameraSetting;
typedef QHash<QString, OnvifCameraSetting> CameraSettings;

class OnvifCameraSettingsResp
{
    DeviceSoapWrapper* m_deviceSoapWrapper;
    ImagingSoapWrapper* m_rangesSoapWrapper;
    ImagingSoapWrapper* m_valsSoapWrapper;
    ImagingOptionsResp* m_rangesResponse;
    ImagingSettingsResp* m_valsResponse;
    const std::string m_videoSrcToken;
    const QString m_uniqId;
    CameraSettings m_cameraSettings;

public:

    OnvifCameraSettingsResp(const std::string& deviceUrl, const std::string& imagingUrl, const QString &login,
        const QString &passwd, const std::string& videoSrcToken, const QString& uniqId, int _timeDrift);

    ~OnvifCameraSettingsResp();

    bool isEmpty() const;
    CameraDiagnostics::Result makeGetRequest();
    bool makeSetRequest();
    const ImagingOptionsResp& getRangesResponse() const;
    const ImagingSettingsResp& getValsResponse() const;
    QString getImagingUrl() const;
    QString getLogin() const;
    QString getPassword() const;
    int getTimeDrift() const;
    QString getUniqueId() const;
    CameraSettings& getCameraSettings() { return m_cameraSettings; }
    DeviceSoapWrapper* getDeviceSoapWrapper();

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
    OnvifCameraSettingOperationAbstract() {}
    virtual ~OnvifCameraSettingOperationAbstract() {}

    static const OnvifCameraSettingOperationAbstract& EMPTY_OPERATION;

    static const QHash<QString, QSharedPointer<OnvifCameraSettingOperationAbstract> > operations;

    virtual bool get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinitSrc = false) const = 0;
    virtual bool set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinitSrc = false) const = 0;

protected:
    inline bool reinitSrc(OnvifCameraSettingsResp& src, bool reinit) const { return !reinit || src.makeGetRequest(); }
    template<class T> bool compareAndSendToCamera(T* field, const T val, OnvifCameraSettingsResp& src) const;
    float calcStep(double min, double max) const;
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
        const QString& description,
        const CameraSettingValue min = CameraSettingValue(),
        const CameraSettingValue max = CameraSettingValue(),
        const CameraSettingValue step = CameraSettingValue(),
        const CameraSettingValue current = CameraSettingValue());

    virtual ~OnvifCameraSetting() {};

    OnvifCameraSetting& operator=(const OnvifCameraSetting& rhs);

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
    OnvifCameraSettingReader(OnvifCameraSettingsResp& onvifSettings);
    virtual ~OnvifCameraSettingReader();

protected:

    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name) override;
    virtual bool isParamEnabled(const QString& id, const QString& parentId) override;
    virtual void paramFound(const CameraSetting& value, const QString& parentId) override;
    virtual void cleanDataOnFail() override;
    virtual void parentOfRootElemFound(const QString& parentId) override;

private:

    OnvifCameraSettingReader();
};

#endif //ENABLE_ONVIF

#endif //onvif_resource_settings_h_2250
