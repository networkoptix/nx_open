#ifndef __FLIR_ONVIF_RESOURCE_H__
#define __FLIR_ONVIF_RESOURCE_H__

#include <plugins/resource/onvif/onvif_resource.h>

class QnFlirOnvifResource : public QnPlOnvifResource
{
    Q_OBJECT
    static int MAX_RESOLUTION_DECREASES_NUM;
public:
    QnFlirOnvifResource();
    //virtual int suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const override;

    virtual CameraDiagnostics::Result initInternal() override;

    /*void fetchAndSetAdvancedParameters();

    virtual bool getParamPhysical(const QString &id, QString &value) override;
    virtual bool getParamsPhysical(const QSet<QString> &idList, QnCameraAdvancedParamValueList& result) override;
    virtual bool setParamPhysical(const QString &id, const QString& value) override;
    virtual bool setParamsPhysical(const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result) override;*/
};

typedef QnSharedResourcePointer<QnFlirOnvifResource> QnFlirOnvifResourcePtr;

#endif //__FLIR_ONVIF_RESOURCE_H__
