#pragma once

#ifdef ENABLE_ARECONT

#include <QtGui/QImage>

#include <nx/mediaserver/resource/camera.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/simple_http_client.h>
#include <nx/streaming/media_data_packet.h>

class QDomElement;

// TODO: #Elric rename class to *ArecontVision*, rename file
class QnPlAreconVisionResource:
    public nx::mediaserver::resource::Camera
{
    Q_OBJECT
    friend class QnPlArecontResourceSearcher;
public:
    static const QString MANUFACTURE;

    QnPlAreconVisionResource();

    CLHttpStatus getRegister(int page, int num, int& val);
    CLHttpStatus setRegister(int page, int num, int val);
    CLHttpStatus setRegister_asynch(int page, int num, int val);

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


    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    virtual bool setRelayOutputState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS) override;

    //virtual QnMediaInfo getMediaInfo() const;

    int totalMdZones() const;
    bool isH264() const;
    int getZoneSite() const;

    QnMetaDataV1Ptr getCameraMetadata();
    QString generateRequestString(
        const QHash<QByteArray, QVariant>& streamParam,
        bool h264,
        bool resolutionFULL,
        bool blackWhite,
        int* const outQuality,
        QSize* const outResolution);

    static QnPlAreconVisionResource* createResourceByName(const QString &name);
    static QnPlAreconVisionResource* createResourceByTypeId(QnUuid rt);

    static bool isPanoramic(QnResourceTypePtr resType);

    virtual bool getApiParameter(const QString &id, QString &value);
    virtual bool setApiParameter(const QString &id, const QString &value);

protected:
    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(bool primaryStream) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setMotionMaskPhysical(int channel) override;
    virtual bool startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler) override;
    virtual void stopInputPortMonitoringAsync() override;

    virtual bool isRTSPSupported() const;

    struct AvParamitersProvider: AdvancedParametersProvider
    {
    public:
        AvParamitersProvider(QnPlAreconVisionResource* resource): m_resource(resource) {}
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
    int m_channelCount;
    int m_prevMotionChannel;
    bool m_dualsensor;
    bool m_inputPortState;
    nx_http::AsyncHttpClientPtr m_relayInputClient;
    AvParamitersProvider m_advancedParamitersProvider;

    bool getParamPhysical2(int channel, const QString& name, QString &val);
    void inputPortStateRequestDone(nx_http::AsyncHttpClientPtr client);
};

typedef QnSharedResourcePointer<QnPlAreconVisionResource> QnPlAreconVisionResourcePtr;

#endif
