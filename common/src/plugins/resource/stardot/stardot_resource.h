#ifndef __STARDOT_RESOURCE_H__
#define __STARDOT_RESOURCE_H__

#ifdef ENABLE_STARDOT

#include <QtCore/QMap>
#include <utils/common/mutex.h>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "utils/network/http/asynchttpclient.h"
#include "utils/network/simple_http_client.h"
#include <utils/network/http/multipartcontentparser.h>

class QnStardotResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnStardotResource();
    ~QnStardotResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool isInitialized() const;

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
    virtual CameraDiagnostics::Result initInternal() override;
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
#endif // __STARDOT_RESOURCE_H__
