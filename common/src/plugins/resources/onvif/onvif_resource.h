#ifndef onvif_resource_h
#define onvif_resource_h

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSharedPointer>
#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"
#include "soap_wrapper.h"
#include "onvif_resource_settings.h"
#include "core/resource/interface/abstract_ptz_controller.h"
#include "onvif_ptz_controller.h"

class onvifXsd__AudioEncoderConfigurationOption;
class onvifXsd__VideoSourceConfigurationOptions;
class onvifXsd__VideoEncoderConfigurationOptions;
class onvifXsd__VideoEncoderConfiguration;
class onvifXsd__VideoSourceConfiguration;
typedef onvifXsd__AudioEncoderConfigurationOption AudioOptions;
typedef onvifXsd__VideoSourceConfigurationOptions VideoSrcOptions;
typedef onvifXsd__VideoEncoderConfigurationOptions VideoOptions;
typedef onvifXsd__VideoEncoderConfiguration VideoEncoder;
typedef onvifXsd__VideoSourceConfiguration VideoSource;


//first = width, second = height
typedef QPair<int, int> ResolutionPair;
const ResolutionPair EMPTY_RESOLUTION_PAIR(0, 0);
const ResolutionPair SECONDARY_STREAM_DEFAULT_RESOLUTION(480, 316); // 316 is average between 272&360
const ResolutionPair SECONDARY_STREAM_MAX_RESOLUTION(720, 480);


class QDomElement;

class QnPlOnvifResource : public QnPhysicalCameraResource
{
public:
    enum CODECS
    {
        H264,
        JPEG
    };

    enum AUDIO_CODECS
    {
        AUDIO_NONE,
        G726,
        G711,
        AAC,
        SIZE_OF_AUDIO_CODECS
    };

    static const char* MANUFACTURE;
    static QString MEDIA_URL_PARAM_NAME;
    static QString ONVIF_URL_PARAM_NAME;
    static QString MAX_FPS_PARAM_NAME;
    static QString AUDIO_SUPPORTED_PARAM_NAME;
    static QString DUAL_STREAMING_PARAM_NAME;
    static const float QUALITY_COEF;
    static const double MAX_SECONDARY_RESOLUTION_SQUARE;
    static const char* PROFILE_NAME_PRIMARY;
    static const char* PROFILE_NAME_SECONDARY;
    static const int MAX_AUDIO_BITRATE;
    static const int MAX_AUDIO_SAMPLERATE;
    static const int ADVANCED_SETTINGS_VALID_TIME; //Time, during which adv settings will not be obtained from camera (in milliseconds)

    static const QString fetchMacAddress(const NetIfacesResp& response, const QString& senderIpAddress);

    QnPlOnvifResource();
    virtual ~QnPlOnvifResource();

    static const QString createOnvifEndpointUrl(const QString& ipAddress);

    virtual bool setHostAddress(const QHostAddress &ip, QnDomain domain = QnDomainMemory) override;


    virtual bool isResourceAccessible() override;
    virtual bool updateMACAddress() override;
    virtual QString manufacture() const override;

    virtual int getMaxFps() override;
    virtual void setIframeDistance(int /*frames*/, int /*timems*/) override {}
    virtual bool hasDualStreaming() const override;
    virtual bool shoudResolveConflicts() const override;

    virtual bool mergeResourcesIfNeeded(QnNetworkResourcePtr source) override;

    virtual int getMaxOnvifRequestTries() const { return 1; };

    int innerQualityToOnvif(QnStreamQuality quality) const;
    const QString createOnvifEndpointUrl() const { return createOnvifEndpointUrl(getHostAddress().toString()); }

    int getGovLength() const;
    int getAudioBitrate() const;
    int getAudioSamplerate() const;
    ResolutionPair getPrimaryResolution() const;
    ResolutionPair getSecondaryResolution() const;
    int getPrimaryH264Profile() const;
    int getSecondaryH264Profile() const;
    ResolutionPair getMaxResolution() const;
    int getTimeDrift() const; // return clock diff between camera and local clock at seconds
    //bool isSoapAuthorized() const;
    const QRect getPhysicalWindowSize() const;
    const QString getPrimaryVideoEncoderId() const;
    const QString getSecondaryVideoEncoderId() const;
    const QString getAudioEncoderId() const;
    const QString getVideoSourceId() const;
    const QString getAudioSourceId() const;


    QString getMediaUrl() const;
    void setMediaUrl(const QString& src);

    QString getImagingUrl() const;
    void setImagingUrl(const QString& src);

    void setPtzfUrl(const QString& src);
    QString getPtzfUrl() const;

    void setDeviceOnvifUrl(const QString& src);

    CODECS getCodec(bool isPrimary) const;
    AUDIO_CODECS getAudioCodec() const;

    const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider);

    void calcTimeDrift(); // calculate clock diff between camera and local clock at seconds
    static int calcTimeDrift(const QString& deviceUrl);


    virtual QnOnvifPtzController* getPtzController() override;
    bool fetchAndSetDeviceInformation();
    QString fromOnvifDiscoveredUrl(const std::string& onvifUrl, bool updatePort = true);

    virtual void setUrl(const QString &url) override;
    int getChannel() const;
protected:
    void setCodec(CODECS c, bool isPrimary);
    void setAudioCodec(AUDIO_CODECS c);

    bool initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual void setCropingPhysical(QRect croping);

    virtual bool updateResourceCapabilities();

    virtual bool getParamPhysical(const QnParam &param, QVariant &val);
    virtual bool setParamPhysical(const QnParam &param, const QVariant& val);

    virtual void fetchAndSetCameraSettings();

    QString getDeviceOnvifUrl() const;
private:
    void setMaxFps(int f);

    bool fetchAndSetResourceOptions();
    void fetchAndSetPrimarySecondaryResolution();
    bool fetchAndSetVideoEncoderOptions(MediaSoapWrapper& soapWrapper);
    void updateSecondaryResolutionList(const VideoOptionsResp& response);
    bool fetchAndSetAudioEncoderOptions(MediaSoapWrapper& soapWrapper);
    bool fetchAndSetDualStreaming(MediaSoapWrapper& soapWrapper);
    bool fetchAndSetAudioEncoder(MediaSoapWrapper& soapWrapper);
    
    bool fetchAndSetVideoSource();
    bool fetchAndSetAudioSource();
    bool fetchAndSetVideoSourceOptions();

    void setVideoEncoderOptions(const VideoOptionsResp& response);
    void setVideoEncoderOptionsH264(const VideoOptionsResp& response);
    void setVideoEncoderOptionsJpeg(const VideoOptionsResp& response);
    void setAudioEncoderOptions(const AudioOptions& options);
    void setVideoSourceOptions(const VideoSrcOptions& options);
    void setMinMaxQuality(int min, int max);

    void save();

    int round(float value);
    ResolutionPair getNearestResolutionForSecondary(const ResolutionPair& resolution, float aspectRatio) const;
    static ResolutionPair getNearestResolution(const ResolutionPair& resolution, float aspectRatio, double maxResolutionSquare, const QList<ResolutionPair>& resolutionList);
    static float getResolutionAspectRatio(const ResolutionPair& resolution);
    int findClosestRateFloor(const std::vector<int>& values, int threshold) const;
    int getH264StreamProfile(const VideoOptionsResp& response);
    void checkMaxFps(VideoConfigsResp& response, const QString& encoderId);
    int sendVideoEncoderToCamera(VideoEncoder& encoder) const;
    void readPtzInfo();

    void updateVideoSource(VideoSource* source) const;
    bool sendVideoSourceToCamera(VideoSource* source) const;
protected:
    QList<ResolutionPair> m_resolutionList; //Sorted desc
    QList<ResolutionPair> m_secondaryResolutionList;
    OnvifCameraSettingsResp* m_onvifAdditionalSettings;

    mutable QMutex m_physicalParamsMutex;
    QDateTime m_advSettingsLastUpdated;

private:
    static const char* ONVIF_PROTOCOL_PREFIX;
    static const char* ONVIF_URL_SUFFIX;
    static const int DEFAULT_IFRAME_DISTANCE;

    QMap<int, QRect> m_motionWindows;
    QMap<int, QRect> m_motionMask;

    int m_iframeDistance;
    int m_minQuality;
    int m_maxQuality;
    CODECS m_primaryCodec;
    CODECS m_secondaryCodec;
    AUDIO_CODECS m_audioCodec;
    ResolutionPair m_primaryResolution;
    ResolutionPair m_secondaryResolution;
    int m_primaryH264Profile;
    int m_secondaryH264Profile;
    int m_audioBitrate;
    int m_audioSamplerate;
    QRect m_physicalWindowSize;
    QString m_primaryVideoEncoderId;
    QString m_secondaryVideoEncoderId;
    QString m_audioEncoderId;
    QString m_videoSourceId;
    QString m_audioSourceId;
    QString m_videoSourceToken;

    bool m_needUpdateOnvifUrl;
    QnOnvifPtzController* m_ptzController;

    QString m_imagingUrl;
    QString m_ptzUrl;
    int m_timeDrift;
    
    int m_channelNumer; // video/audio source number
    int m_maxChannels;
};

#endif //onvif_resource_h
