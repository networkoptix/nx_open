#pragma once

#ifdef ENABLE_STARDOT

#include <QtCore/QMap>
#include <nx/utils/thread/mutex.h>

#include <nx/mediaserver/resource/camera.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/deprecated/simple_http_client.h>

class QnStardotResource: public nx::mediaserver::resource::Camera
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnStardotResource();
    ~QnStardotResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;
    virtual bool hasDualStreaming() const override;
    virtual int getMaxFps() const override;

    QString getRtspUrl() const;

    QByteArray makeStardotRequest(const QString& request, CLHttpStatus& status) const;
    QByteArray makeStardotPostRequest(const QString& request, const QString& body, CLHttpStatus& status) const;

    QSize getResolution() const;

    bool isAudioSupported() const;

    simd128i* getMotionMaskBinData() const;

protected:
    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setMotionMaskPhysical(int channel) override;
private:
    QSize extractResolution(const QByteArray& resolutionStr) const;
    void detectMaxResolutionAndFps(const QByteArray& resList);
    void parseInfo(const QByteArray& info);
private:
    bool m_hasAudio;
    QSize m_resolution; // index 0 for primary, index 1 for secondary
    int m_resolutionNum; // internal camera resolution number
    int m_maxFps;
    int m_rtspPort;
    QString m_rtspTransport;
    simd128i *m_motionMaskBinData;
};

#endif // #ifdef ENABLE_STARDOT
