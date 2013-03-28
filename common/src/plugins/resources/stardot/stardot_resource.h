#ifndef __STARDOT_RESOURCE_H__
#define __STARDOT_RESOURCE_H__

#include <QMap>
#include <QMutex>

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
    static const char* MANUFACTURE;

    QnStardotResource();
    ~QnStardotResource();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool isInitialized() const;

    virtual bool shoudResolveConflicts() const override;

    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) override;
    virtual bool hasDualStreaming() const override;
    virtual int getMaxFps() override;

    QString getRtspUrl() const;

    QByteArray makeStardotRequest(const QString& request, CLHttpStatus& status) const;
    QSize getResolution() const;

    bool isAudioSupported() const;

    __m128i* getMotionMaskBinData() const;
signals:
    //!Emitted on camera input port state has been changed
    /*!
        \param resource Smart pointer to \a this
        \param inputPortID
        \param value true if input is connected, false otherwise
        \param timestamp MSecs since epoch, UTC
    */
    void cameraInput(
        QnResourcePtr resource,
        const QString& inputPortID,
        bool value,
        qint64 timestamp);

protected:
    bool initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCropingPhysical(QRect croping) override;
    virtual bool isResourceAccessible() override;
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
    __m128i *m_motionMaskBinData;
};

#endif // __STARDOT_RESOURCE_H__
