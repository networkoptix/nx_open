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
    typedef std::function<float* (ImagingSettingsResp* settings)> pathFunction;

    QnFloatOnvifImagingOperation(ImagingSettingsResp* settings, pathFunction path);
    virtual ~QnFloatOnvifImagingOperation();

    virtual bool get(QString& value) const override;
    virtual bool set(const QString& value) const override;
private:
    pathFunction m_path;
};

class QnEnumOnvifImagingOperation: public QnAbstractOnvifImagingOperation {
public:
    typedef std::function<int* (ImagingSettingsResp* settings)> pathFunction;

    QnEnumOnvifImagingOperation(ImagingSettingsResp* settings, const QStringList &range, pathFunction path);
    virtual ~QnEnumOnvifImagingOperation();

    virtual bool get(QString& value) const override;
    virtual bool set(const QString& value) const override;
private:
    pathFunction m_path;
    QStringList m_range;
};

#endif //ENABLE_ONVIF

#endif //QN_ONVIF_IMAGING_SETTINGS_H