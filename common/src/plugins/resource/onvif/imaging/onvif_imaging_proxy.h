#ifndef QN_ONVIF_IMAGING_PROXY_H
#define QN_ONVIF_IMAGING_PROXY_H

#ifdef ENABLE_ONVIF

#include <core/resource/camera_advanced_param.h>

#include <plugins/resource/onvif/soap_wrapper.h>
#include <plugins/resource/onvif/imaging/onvif_imaging_settings.h>

#include <utils/camera/camera_diagnostics.h>

//
// QnOnvifImagingProxy
//
class QnOnvifImagingProxy {
public:
    QnOnvifImagingProxy(const std::string& imagingUrl, const QString &login,
        const QString &passwd, const std::string& videoSrcToken, int _timeDrift);
    ~QnOnvifImagingProxy();

    void initParameters(QnCameraAdvancedParams &parameters);
    QSet<QString> supportedParameters() const;

    bool setValue(const QString &id, const QString &value);
    CameraDiagnostics::Result loadValues(QnCameraAdvancedParamValueMap &values);
private:
    CameraDiagnostics::Result loadRanges();
    bool makeSetRequest();

    QString getImagingUrl() const;
    QString getLogin() const;
    QString getPassword() const;
    int getTimeDrift() const;

private:
    ImagingSoapWrapper* m_rangesSoapWrapper;
    ImagingSoapWrapper* m_valsSoapWrapper;
    ImagingOptionsResp* m_ranges;
    ImagingSettingsResp* m_values;
    const std::string m_videoSrcToken;

    QHash<QString, QnAbstractOnvifImagingOperationPtr > m_supportedOperations;
};

#endif //ENABLE_ONVIF

#endif //QN_ONVIF_IMAGING_PROXY_H
