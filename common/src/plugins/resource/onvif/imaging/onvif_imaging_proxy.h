#ifndef QN_ONVIF_IMAGING_PROXY_H
#define QN_ONVIF_IMAGING_PROXY_H

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/soap_wrapper.h>

#include <utils/camera/camera_diagnostics.h>

#include <core/resource/camera_advanced_param.h>

class QnAbstractOnvifImagingOperation;

//
// QnOnvifImagingProxy
//
class QnOnvifImagingProxy {
public:
    QnOnvifImagingProxy(const std::string& imagingUrl, const QString &login,
        const QString &passwd, const std::string& videoSrcToken, int _timeDrift);
    ~QnOnvifImagingProxy();

    bool makeSetRequest();
    QString getImagingUrl() const;
    QString getLogin() const;
    QString getPassword() const;
    int getTimeDrift() const;

    void initParameters(QnCameraAdvancedParams &parameters);
    QSet<QString> supportedParameters() const;

    CameraDiagnostics::Result loadValues(QnCameraAdvancedParamValueMap &values);
private:
    CameraDiagnostics::Result loadRanges();
    

private:
    ImagingSoapWrapper* m_rangesSoapWrapper;
    ImagingSoapWrapper* m_valsSoapWrapper;
    ImagingOptionsResp* m_ranges;
    ImagingSettingsResp* m_values;
    const std::string m_videoSrcToken;

    QHash<QString, QSharedPointer<QnAbstractOnvifImagingOperation> > m_supportedOperations;
};

#endif //ENABLE_ONVIF

#endif //QN_ONVIF_IMAGING_PROXY_H
