#ifndef onvif_2_1_1_resource_h
#define onvif_2_1_1_resource_h

#include <QList>
#include <QMap>
#include <QPair>
#include <QSharedPointer>
#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"

class _onvifMedia__GetVideoEncoderConfigurationOptionsResponse;
//class _onvifMedia__GetVideoEncoderConfigurationsResponse;
struct VideoEncoders;
class _onvifMedia__GetProfilesResponse;

//first = width, second = height
typedef QPair<int, int> ResolutionPair;
const ResolutionPair EMPTY_RESOLUTION_PAIR(0, 0);

class QnPlOnvifGeneric211Resource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;
    static const QString& MEDIA_URL_PARAM_NAME;
    static const QString& DEVICE_URL_PARAM_NAME;

    QnPlOnvifGeneric211Resource();

    static const QString createOnvifEndpointUrl(const QString& ipAddress);

    virtual bool isResourceAccessible() override;
    virtual bool updateMACAddress() override;
    virtual QString manufacture() const override;
    virtual void setMotionMaskPhysical(int channel) override;
    virtual int getMaxFps() override;
    virtual void setIframeDistance(int /*frames*/, int /*timems*/) override {}
    virtual bool hasDualStreaming() const override { return hasDual; }

    bool isInitialized() const;

    int innerQualityToOnvif(QnStreamQuality quality) const;
    const QString createOnvifEndpointUrl() const { return createOnvifEndpointUrl(getHostAddress().toString()); }

    const ResolutionPair getMaxResolution() const;
    const ResolutionPair getNearestResolution(const ResolutionPair& resolution, float aspectRatio) const;
    float getResolutionAspectRatio(const ResolutionPair& resolution) const;
    bool isVideoOptionsNotSet() const { return videoOptionsNotSet; }
    bool isSoapAuthorized() const;

    QRect getMotionWindow(int num) const;
    QMap<int, QRect>  getMotionWindows() const;
    void readMotionInfo();

    const QString& getMediaUrl() const { return mediaUrl; }
    void setMediaUrl(const QString& src) { mediaUrl = src; }

    const QString& getDeviceUrl() const { return deviceUrl; }
    void setDeviceUrl(const QString& src) { deviceUrl = src; }

protected:
    void init();
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual void setCropingPhysical(QRect croping);
private:
    void clear();
    static QRect axisRectToGridRect(const QRect& axisRect);
    static QRect gridRectToAxisRect(const QRect& gridRect);

    bool removeMotionWindow(int wndNum);
    int addMotionWindow();
    bool updateMotionWindow(int wndNum, int sensitivity, const QRect& rect);
    int toAxisMotionSensitivity(int sensitivity);
    void fetchAndSetVideoEncoderOptions();
    void setVideoEncoderOptions(const _onvifMedia__GetVideoEncoderConfigurationOptionsResponse& response);
    void analyzeVideoEncoders(VideoEncoders& encoders, bool setOptions);
    int countAppropriateProfiles(const _onvifMedia__GetProfilesResponse& response, VideoEncoders& encoders);
private:
    static const char* ONVIF_PROTOCOL_PREFIX;
    static const char* ONVIF_URL_SUFFIX;
    static const int DEFAULT_IFRAME_DISTANCE;

    QList<ResolutionPair> m_resolutionList; //Sorted desc
    QMap<int, QRect> m_motionWindows;
    QMap<int, QRect> m_motionMask;
    qint64 m_lastMotionReadTime;
    int maxFps;
    int iframeDistance;
    int minQuality;
    int maxQuality;
    bool hasDual;
    bool videoOptionsNotSet;
    QString mediaUrl;
    QString deviceUrl;
};

typedef QSharedPointer<QnPlOnvifGeneric211Resource> QnPlOnvifGeneric211ResourcePtr;

#endif //onvif_2_1_1_resource_h
