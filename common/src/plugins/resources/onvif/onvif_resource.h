#ifndef onvif_resource_h
#define onvif_resource_h

#include <list>
#include <stack>

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSharedPointer>
#include <QXmlDefaultHandler>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"
#include "soap_wrapper.h"
#include "onvif_resource_settings.h"
#include "core/resource/interface/abstract_ptz_controller.h"
#include "onvif_ptz_controller.h"
#include "utils/common/timermanager.h"
#include "utils/common/systemtimer.h"


class onvifXsd__AudioEncoderConfigurationOption;
class onvifXsd__VideoSourceConfigurationOptions;
class onvifXsd__VideoEncoderConfigurationOptions;
class onvifXsd__VideoEncoderConfiguration;
class onvifXsd__EventCapabilities;
class oasisWsnB2__NotificationMessageHolderType;
typedef onvifXsd__AudioEncoderConfigurationOption AudioOptions;
typedef onvifXsd__VideoSourceConfigurationOptions VideoSrcOptions;
typedef onvifXsd__VideoEncoderConfigurationOptions VideoOptions;
typedef onvifXsd__VideoEncoderConfiguration VideoEncoder;

//first = width, second = height
typedef QPair<int, int> ResolutionPair;
const ResolutionPair EMPTY_RESOLUTION_PAIR(0, 0);
const ResolutionPair SECONDARY_STREAM_DEFAULT_RESOLUTION(480, 316); // 316 is average between 272&360
const ResolutionPair SECONDARY_STREAM_MAX_RESOLUTION(640, 480);


class QDomElement;

struct CameraPhysicalWindowSize
{
    int x;
    int y;
    int width;
    int height;

    CameraPhysicalWindowSize(): x(0), y(0), width(0), height(0) {}

    bool isValid()
    {
        return x >= 0 && y >=0 && width > 0 && height > 0;
    }
};

class QnPlOnvifResource
:
    public QnPhysicalCameraResource,
    public TimerEventHandler
{
    Q_OBJECT

public:
    class RelayOutputInfo
    {
    public:
        std::string token;
        bool isBistable;
        //!Valid only if \a isBistable is \a false
        std::string delayTime;
        bool activeByDefault;

        RelayOutputInfo();
        RelayOutputInfo(
            const std::string& _token,
            bool _isBistable,
            const std::string& _delayTime,
            bool _activeByDefault );
    };

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

    virtual bool setHostAddress(const QString &ip, QnDomain domain = QnDomainMemory) override;


    virtual bool isResourceAccessible() override;
    virtual bool updateMACAddress() override;
    virtual QString manufacture() const override;

    virtual int getMaxFps() override;
    virtual void setIframeDistance(int /*frames*/, int /*timems*/) override {}
    virtual bool hasDualStreaming() const override;
    virtual bool shoudResolveConflicts() const override;

    virtual bool mergeResourcesIfNeeded(QnNetworkResourcePtr source) override;

    virtual int getMaxOnvifRequestTries() const { return 1; };

    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getRelayOutputList() const override;
    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getInputPortList() const override;
    //!Implementation of QnSecurityCamResource::setRelayOutputState
    virtual bool setRelayOutputState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;

    int innerQualityToOnvif(QnStreamQuality quality) const;
    const QString createOnvifEndpointUrl() const { return createOnvifEndpointUrl(getHostAddress()); }

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
    const CameraPhysicalWindowSize getPhysicalWindowSize() const;
    const QString getPrimaryVideoEncoderId() const;
    const QString getSecondaryVideoEncoderId() const;
    const QString getAudioEncoderId() const;
    const QString getVideoSourceId() const;
    const QString getAudioSourceId() const;


    QString getMediaUrl() const;
    void setMediaUrl(const QString& src);

    QString getImagingUrl() const;
    void setImagingUrl(const QString& src);

    QString getPtzfUrl() const;
    void setPtzfUrl(const QString& src);

    QString getDeviceOnvifUrl() const;
    void setDeviceOnvifUrl(const QString& src);

    CODECS getCodec(bool isPrimary) const;
    AUDIO_CODECS getAudioCodec() const;

    const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider);

    bool forcePrimaryEncoderCodec() const;
    void calcTimeDrift(); // calculate clock diff between camera and local clock at seconds

    virtual QnOnvifPtzController* getPtzController() override;
    bool fetchAndSetDeviceInformation();

    //!Relay input with token \a relayToken has changed its state to \a active
    //void notificationReceived( const std::string& relayToken, bool active );
    void notificationReceived( const oasisWsnB2__NotificationMessageHolderType& notification );
    //!Implementation of TimerEventHandler::onTimer
    virtual void onTimer( const quint64& timerID );
    QString fromOnvifDiscoveredUrl(const std::string& onvifUrl, bool updatePort = true);

signals:
    void cameraInput(
        QnResourcePtr resource,
        const QString& inputPortID,
        bool value,
        qint64 timestamp );

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

private:
    void setMaxFps(int f);

    bool fetchAndSetResourceOptions();
    void fetchAndSetPrimarySecondaryResolution();
    bool fetchAndSetVideoEncoderOptions(MediaSoapWrapper& soapWrapper);
    void updateSecondaryResolutionList(const VideoOptionsResp& response);
    bool fetchAndSetAudioEncoderOptions(MediaSoapWrapper& soapWrapper);
    bool fetchAndSetVideoSourceOptions(MediaSoapWrapper& soapWrapper);
    bool fetchAndSetDualStreaming(MediaSoapWrapper& soapWrapper);
    bool fetchAndSetAudioEncoder(MediaSoapWrapper& soapWrapper);
    bool fetchAndSetVideoSource(MediaSoapWrapper& soapWrapper);
    bool fetchAndSetAudioSource(MediaSoapWrapper& soapWrapper);

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

protected:
    QList<ResolutionPair> m_resolutionList; //Sorted desc
    QList<ResolutionPair> m_secondaryResolutionList;
    OnvifCameraSettingsResp* m_onvifAdditionalSettings;

    mutable QMutex m_physicalParamsMutex;
    QDateTime m_advSettingsLastUpdated;

private slots:
    void onRenewSubscriptionTimer();

private:
    enum EventMonitorType
    {
        emtNone,
        emtNotification,
        emtPullPoint
    };

    //!Parses <dom0:SubscriptionId xmlns:dom0=\"(null)\">1</dom0:SubscriptionId>
    class SubscriptionReferenceParametersParseHandler
    :
        public QXmlDefaultHandler
    {
    public:
        QString subscriptionID;

        SubscriptionReferenceParametersParseHandler();

        virtual bool characters( const QString& ch ) override;
        virtual bool startElement( const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& atts ) override;
        virtual bool endElement( const QString& namespaceURI, const QString& localName, const QString& qName ) override;

    private:
        bool m_readingSubscriptionID;
    };

    //!Parses tt:Message element (onvif-core-specification, 9.11.6)
    class NotificationMessageParseHandler
    :
        public QXmlDefaultHandler
    {
    public:
        class SimpleItem
        {
        public:
            QString name;
            QString value;

            SimpleItem()
            {
            }

            SimpleItem( const QString& _name, const QString& _value )
            :
                name( _name ),
                value( _value )
            {
            }
        };

        QDateTime utcTime;
        std::list<SimpleItem> source;
        SimpleItem data;

        NotificationMessageParseHandler();

        virtual bool startElement( const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& atts ) override;
        virtual bool endElement( const QString& namespaceURI, const QString& localName, const QString& qName ) override;

    private:
        enum State
        {
            init,
            readingMessage,
            readingSource,
            readingSourceItem,
            readingData,
            readingDataItem
        };

        std::stack<State> m_parseStateStack;
    };

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
    CameraPhysicalWindowSize m_physicalWindowSize;
    QString m_primaryVideoEncoderId;
    QString m_secondaryVideoEncoderId;
    QString m_audioEncoderId;
    QString m_videoSourceId;
    QString m_audioSourceId;
    QString m_videoSourceToken;

    bool m_needUpdateOnvifUrl;
    bool m_forceCodecFromPrimaryEncoder;
    QnOnvifPtzController* m_ptzController;

    QString m_imagingUrl;
    QString m_ptzUrl;
    int m_timeDrift;
    int m_prevSoapCallResult;
    onvifXsd__EventCapabilities* m_eventCapabilities;
    std::vector<RelayOutputInfo> m_relayOutputInfo;
    std::map<QString, bool> m_relayInputStates;
    std::string m_deviceIOUrl;
    QString m_onvifNotificationSubscriptionID;
    QMutex m_subscriptionMutex;
    EventMonitorType m_eventMonitorType;
    quint64 m_timerID;

    bool registerNotificationConsumer();
    bool createPullPointSubscription();
    bool pullMessages();
    //!Registeres local NotificationConsumer in resource's NotificationProducer
    bool registerNotificationHandler();
    //!Reads relay output list from resource
    bool fetchRelayOutputs( std::vector<RelayOutputInfo>* const relayOutputs );
    bool fetchRelayOutputInfo( const std::string& outputID, RelayOutputInfo* const relayOutputInfo );
    bool fetchRelayInputInfo();
    bool setRelayOutputSettings( const RelayOutputInfo& relayOutputInfo );
};

#endif //onvif_resource_h
