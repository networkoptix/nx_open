#pragma once

#ifdef ENABLE_ARECONT

#include <QtGui/QImage>

#include <nx/vms/server/resource/camera.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/network/http/http_async_client.h>

class QDomElement;
class QnMediaServerModule;

// TODO: #Elric rename class to *ArecontVision*, rename file
class QnPlAreconVisionResource:
    public nx::vms::server::resource::Camera
{
    Q_OBJECT
    friend class QnPlArecontResourceSearcher;
public:
    static const QString MANUFACTURE;

    QnPlAreconVisionResource(QnMediaServerModule* serverModule);

    CLHttpStatus getRegister(int page, int num, int& val);
    CLHttpStatus setRegister(int page, int num, int val);

    bool isPanoramic() const;
    bool isDualSensor() const;

    virtual void setHostAddress(const QString& ip) override;

    virtual bool getDescription() {return true;};

    //!Implementation of QnNetworkResource::ping
    virtual bool ping() override;
    //!Implementation of QnNetworkResource::checkIfOnlineAsync
    virtual void checkIfOnlineAsync( std::function<void(bool)> completionHandler ) override;

    virtual QString getDriverName() const override;

    virtual Qn::StreamQuality getBestQualityForSuchOnScreenSize(const QSize& size) const override;


    virtual QImage getImage(int channnel, QDateTime time, Qn::StreamQuality quality) const override;

    virtual bool setOutputPortState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS) override;

    //virtual QnMediaInfo getMediaInfo() const;

    int totalMdZones() const;
    bool isH264() const;
    int getZoneSite() const;

    QString generateRequestString(
        const QHash<QByteArray, QVariant>& streamParam,
        bool h264,
        bool resolutionFULL,
        bool blackWhite,
        int* const outQuality,
        QSize* const outResolution);

    static QnPlAreconVisionResource* createResourceByName(
        QnMediaServerModule* serverModule, const QString &name);
    static QnPlAreconVisionResource* createResourceByTypeId(
        QnMediaServerModule* serverModule, QnUuid rt);

    static bool isPanoramic(QnResourceTypePtr resType);

    virtual bool getApiParameter(const QString &id, QString &value);
    virtual bool setApiParameter(const QString &id, const QString &value);

protected:
    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setMotionMaskPhysical(int channel) override;
    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;

    virtual bool isRTSPSupported() const;

    struct AvParametersProvider: AdvancedParametersProvider
    {
    public:
        AvParametersProvider(QnPlAreconVisionResource* resource): m_resource(resource) {}
        virtual QnCameraAdvancedParams descriptions() override;
        virtual QnCameraAdvancedParamValueMap get(const QSet<QString>& ids) override;
        virtual QSet<QString> set(const QnCameraAdvancedParamValueMap& values) override;

    private:
        QnPlAreconVisionResource* m_resource = nullptr;
    };

    virtual std::vector<Camera::AdvancedParametersProvider*> advancedParametersProviders() override;

protected:
    QString m_description;

private:
    int m_totalMdZones;
    int m_zoneSite;
    bool m_dualsensor;
    bool m_inputPortState;
    nx::network::http::AsyncHttpClientPtr m_relayInputClient;

    AvParametersProvider m_advancedParametersProvider;

    void inputPortStateRequestDone(nx::network::http::AsyncHttpClientPtr client);
};

#endif
