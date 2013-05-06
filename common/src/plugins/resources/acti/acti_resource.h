#ifndef __ACTI_RESOURCE_H__
#define __ACTI_RESOURCE_H__

#include <QMap>
#include <QMutex>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "utils/network/http/asynchttpclient.h"
#include "utils/network/simple_http_client.h"
#include <utils/network/http/multipartcontentparser.h>

class QnActiPtzController;

class QnActiResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const char* MANUFACTURE;

    static const int MAX_STREAMS = 2;

    QnActiResource();
    ~QnActiResource();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool isInitialized() const;

    virtual bool shoudResolveConflicts() const override;

    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) override;
    virtual bool hasDualStreaming() const override;
    virtual int getMaxFps() override;

    QString getRtspUrl(int actiChannelNum) const; // in range 1..N

    QByteArray makeActiRequest(const QString& group, const QString& command, CLHttpStatus& status, bool keepAllData = false) const;
    QSize getResolution(QnResource::ConnectionRole role) const;
    int roundFps(int srcFps, QnResource::ConnectionRole role) const;
    int roundBitrate(int srcBitrateKbps) const;

    bool isAudioSupported() const;
    virtual QnAbstractPtzController* getPtzController() override;

    static QByteArray unquoteStr(const QByteArray& value);
    QMap<QByteArray, QByteArray> parseReport(const QByteArray& report) const;
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
    virtual bool isResourceAccessible();
private:
    QSize extractResolution(const QByteArray& resolutionStr) const;
    QList<QSize> parseResolutionStr(const QByteArray& resolutions);
    QList<int> parseVideoBitrateCap(const QByteArray& bitrateCap) const;
    void initializePtz();
private:
    bool m_hasAudio;
    QSize m_resolution[MAX_STREAMS]; // index 0 for primary, index 1 for secondary
    QList<int> m_availFps[MAX_STREAMS];
    QList<int> m_availBitrate;
    int m_rtspPort;
    QScopedPointer<QnActiPtzController> m_ptzController;
};

#endif // __ACTI_RESOURCE_H__
