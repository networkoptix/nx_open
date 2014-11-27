#ifndef QN_ONVIF_IMAGING_PROXY_H
#define QN_ONVIF_IMAGING_PROXY_H

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/soap_wrapper.h>

#include <utils/camera/camera_diagnostics.h>

#include <core/resource/camera_advanced_param.h>

class OnvifCameraSettingOperationAbstract;

//
// OnvifCameraSettingsResp
//
class OnvifCameraSettingsResp {
public:
    OnvifCameraSettingsResp(const std::string& imagingUrl, const QString &login,
        const QString &passwd, const std::string& videoSrcToken, int _timeDrift);
    ~OnvifCameraSettingsResp();

    bool makeSetRequest();
    QString getImagingUrl() const;
    QString getLogin() const;
    QString getPassword() const;
    int getTimeDrift() const;

    void initParameters(QnCameraAdvancedParams &parameters);

private:
    CameraDiagnostics::Result loadRanges();
    CameraDiagnostics::Result loadValues();

private:
    ImagingSoapWrapper* m_rangesSoapWrapper;
    ImagingSoapWrapper* m_valsSoapWrapper;
    ImagingOptionsResp* m_ranges;
    ImagingSettingsResp* m_values;
    const std::string m_videoSrcToken;

    QHash<QString, QSharedPointer<OnvifCameraSettingOperationAbstract> > m_supportedOperations;
};

#endif //ENABLE_ONVIF

#endif //QN_ONVIF_IMAGING_PROXY_H
