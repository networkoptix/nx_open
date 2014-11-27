#ifndef QN_ONVIF_IMAGING_SETTINGS_H
#define QN_ONVIF_IMAGING_SETTINGS_H

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/soap_wrapper.h>

//
// OnvifCameraSettingOperation
//
class QnAbstractOnvifImagingOperation {
public:
    QnAbstractOnvifImagingOperation(ImagingSettingsResp* settings);
    virtual ~QnAbstractOnvifImagingOperation();

    virtual bool get(QString& value) const = 0;
    virtual bool set(const QString& value) const = 0;
protected:
    ImagingSettingsResp* m_settings;
};

class QnFloatOnvifImagingOperation: public QnAbstractOnvifImagingOperation {
public:
    QnFloatOnvifImagingOperation(ImagingSettingsResp* settings, std::function<float* (ImagingSettingsResp* settings)> path);
    virtual ~QnFloatOnvifImagingOperation();

    virtual bool get(QString& value) const override;
    virtual bool set(const QString& value) const override;
private:
    std::function<float* (ImagingSettingsResp* settings)> m_path;
};

class ImagingWhiteBalanceYbGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposureIrisOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposureExposureTimeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingWideDynamicRangeModeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingWhiteBalanceModeOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& AUTO_STR;
    static const QString& MANUAL_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposureMaxGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposureMinExposureTimeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingBacklightCompensationLevelOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingContrastOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposurePriorityOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& LOW_NOISE_STR;
    static const QString& FRAME_RATE_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingSharpnessOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingIrCutFilterModesOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& AUTO_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingWhiteBalanceYrGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposureMinGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposureGainOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposureModeOperation: public OnvifCameraSettingOperationAbstract
{
    static const QString& AUTO_STR;
    static const QString& MANUAL_STR;
    static const QString& ALL_VALUES_STR;

public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingWideDynamicRangeLevelOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposureMaxIrisOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingBrightnessOperation: public OnvifCameraSettingOperationAbstract {
public:
    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposureMinIrisOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingBacklightCompensationModeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingColorSaturationOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class ImagingExposureMaxExposureTimeOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class MaintenanceSystemRebootOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class MaintenanceSoftSystemFactoryDefaultOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

class MaintenanceHardSystemFactoryDefaultOperation: public OnvifCameraSettingOperationAbstract
{
public:

    virtual bool get(QString &value) const override;
    virtual bool set(const QString &value) const override;
};

#endif //ENABLE_ONVIF

#endif //QN_ONVIF_IMAGING_SETTINGS_H