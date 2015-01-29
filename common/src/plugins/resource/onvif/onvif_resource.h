#ifndef onvif_resource_h
#define onvif_resource_h

#ifdef ENABLE_ONVIF

#include <list>
#include <memory>
#include <stack>

#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QSharedPointer>
#include <QtCore/QWaitCondition>
#include <QtXml/QXmlDefaultHandler>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"
#include "soap_wrapper.h"
#include "onvif_resource_settings.h"


class onvifXsd__AudioEncoderConfigurationOption;
class onvifXsd__VideoSourceConfigurationOptions;
class onvifXsd__VideoEncoderConfigurationOptions;
class onvifXsd__VideoEncoderConfiguration;
class oasisWsnB2__NotificationMessageHolderType;
class onvifXsd__VideoSourceConfiguration;
class onvifXsd__EventCapabilities;
typedef onvifXsd__AudioEncoderConfigurationOption AudioOptions;
typedef onvifXsd__VideoSourceConfigurationOptions VideoSrcOptions;
typedef onvifXsd__VideoEncoderConfigurationOptions VideoOptions;
typedef onvifXsd__VideoEncoderConfiguration VideoEncoder;
typedef onvifXsd__VideoSourceConfiguration VideoSource;

class VideoOptionsLocal;

//first = width, second = height

class QDomElement;

template<typename SyncWrapper, typename Request, typename Response>
class GSoapAsyncCallWrapper;

/*
* This structure is used during discovery process. These data are read by getDeviceInformation request and may override data from multicast packet
*/
struct OnvifResExtInfo
{
    QString name;
    QString model;
    QString firmware;
    QString vendor;
    QString hardwareId;
    QString serial;
    QString mac;
};

class QnPlOnvifResource
:
    public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    typedef GSoapAsyncCallWrapper <
        PullPointSubscriptionWrapper,
        _onvifEvents__PullMessages,
        _onvifEvents__PullMessagesResponse
    > GSoapAsyncPullMessagesCallWrapper;

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
        AMR,
        SIZE_OF_AUDIO_CODECS // TODO: #Elric #enum
    };

    static const QString MANUFACTURE;
    static QString MEDIA_URL_PARAM_NAME;
    static QString ONVIF_URL_PARAM_NAME;
    static QString ONVIF_ID_PARAM_NAME;
    static const float QUALITY_COEF;
    static const int MAX_AUDIO_BITRATE;
    static const int MAX_AUDIO_SAMPLERATE;
    static const int ADVANCED_SETTINGS_VALID_TIME; //Time, during which adv settings will not be obtained from camera (in milliseconds)

    static const QString fetchMacAddress(const NetIfacesResp& response, const QString& senderIpAddress);

    QnPlOnvifResource();
    virtual ~QnPlOnvifResource();

    static const QString createOnvifEndpointUrl(const QString& ipAddress);

    virtual void setHostAddress(const QString &ip) override;


    //!Implementation of QnNetworkResource::checkIfOnlineAsync
    virtual bool checkIfOnlineAsync( std::function<void(bool)>&& completionHandler ) override;

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int /*frames*/, int /*timems*/) override {}

    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source) override;

    virtual int getMaxOnvifRequestTries() const { return 1; }

    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getRelayOutputList() const override;
    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getInputPortList() const override;
    //!Implementation of QnSecurityCamResource::setRelayOutputState
    /*!
        Actual request is performed asynchronously. This method only posts task to the queue
    */
    virtual bool setRelayOutputState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;

    int innerQualityToOnvif(Qn::StreamQuality quality) const;
    const QString createOnvifEndpointUrl() const { return createOnvifEndpointUrl(getHostAddress()); }

    int getGovLength() const;
    int getAudioBitrate() const;
    int getAudioSamplerate() const;
    QSize getPrimaryResolution() const;
    QSize getSecondaryResolution() const;
    int getPrimaryH264Profile() const;
    int getSecondaryH264Profile() const;
    QSize getMaxResolution() const;
    int getTimeDrift() const; // return clock diff between camera and local clock at seconds
    void setTimeDrift(int value); // return clock diff between camera and local clock at seconds
    //bool isSoapAuthorized() const;
    const QSize getVideoSourceSize() const;

    const QString getPrimaryVideoEncoderId() const;
    const QString getSecondaryVideoEncoderId() const;
    const QString getAudioEncoderId() const;
    const QString getVideoSourceId() const;
    const QString getAudioSourceId() const;

    void updateOnvifUrls(const QnPlOnvifResourcePtr& other);


    QString getMediaUrl() const;
    void setMediaUrl(const QString& src);

    QString getImagingUrl() const;
    void setImagingUrl(const QString& src);

    QString getVideoSourceToken() const;
    void setVideoSourceToken(const QString &src);

    QString getPtzUrl() const;
    void setPtzUrl(const QString& src);

    QString getPtzConfigurationToken() const;
    void setPtzConfigurationToken(const QString &src);

    QString getPtzProfileToken() const;
    void setPtzProfileToken(const QString& src); 

    QString getDeviceOnvifUrl() const;
    void setDeviceOnvifUrl(const QString& src);

    CODECS getCodec(bool isPrimary) const;
    AUDIO_CODECS getAudioCodec() const;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;

    void calcTimeDrift(); // calculate clock diff between camera and local clock at seconds
    static int calcTimeDrift(const QString& deviceUrl);


    virtual QnAbstractPtzController *createPtzControllerInternal() override;
    //bool fetchAndSetDeviceInformation(bool performSimpleCheck);
    static CameraDiagnostics::Result readDeviceInformation(const QString& onvifUrl, const QAuthenticator& auth, int timeDrift, OnvifResExtInfo* extInfo);
    CameraDiagnostics::Result readDeviceInformation();
    CameraDiagnostics::Result getFullUrlInfo();


    //!Relay input with token \a relayToken has changed its state to \a active
    //void notificationReceived( const std::string& relayToken, bool active );
    /*!
        Notifications with timestamp earlier than \a minNotificationTime are ignored
    */
    void notificationReceived(
        const oasisWsnB2__NotificationMessageHolderType& notification,
        time_t minNotificationTime = (time_t)-1 );
    QString fromOnvifDiscoveredUrl(const std::string& onvifUrl, bool updatePort = true);

    int getMaxChannels() const;

    void updateToChannel(int value);

    bool detectVideoSourceCount();

    CameraDiagnostics::Result sendVideoEncoderToCamera(VideoEncoder& encoder);
    bool secondaryResolutionIsLarge() const;
    virtual int suggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps) const override;

    QMutex* getStreamConfMutex();
    void beforeConfigureStream();
    void afterConfigureStream();

    static QSize findSecondaryResolution(const QSize& primaryRes, const QList<QSize>& secondaryResList, double* matchCoeff = 0);

protected:
    int strictBitrate(int bitrate) const;
    void setCodec(CODECS c, bool isPrimary);
    void setAudioCodec(AUDIO_CODECS c);

    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual void setCroppingPhysical(QRect cropping);

    virtual CameraDiagnostics::Result updateResourceCapabilities();

    virtual bool getParamPhysical(const QString &param, QVariant &val);
    virtual bool setParamPhysical(const QString &param, const QVariant& val);

    virtual void fetchAndSetCameraSettings();

private:
    void setMaxFps(int f);

    CameraDiagnostics::Result fetchAndSetResourceOptions();
    void fetchAndSetPrimarySecondaryResolution();
    CameraDiagnostics::Result fetchAndSetVideoEncoderOptions(MediaSoapWrapper& soapWrapper);
    void updateSecondaryResolutionList(const VideoOptionsLocal& opts);
    bool fetchAndSetAudioEncoderOptions(MediaSoapWrapper& soapWrapper);
    bool fetchAndSetDualStreaming(MediaSoapWrapper& soapWrapper);
    bool fetchAndSetAudioEncoder(MediaSoapWrapper& soapWrapper);
    
    CameraDiagnostics::Result fetchVideoSourceToken();
    CameraDiagnostics::Result fetchAndSetVideoSource();
    CameraDiagnostics::Result fetchAndSetAudioSource();

    void setVideoEncoderOptions(const VideoOptionsLocal& opts);
    void setVideoEncoderOptionsH264(const VideoOptionsLocal& opts);
    void setVideoEncoderOptionsJpeg(const VideoOptionsLocal& opts);
    void setAudioEncoderOptions(const AudioOptions& options);
    void setVideoSourceOptions(const VideoSrcOptions& options);
    void setMinMaxQuality(int min, int max);

    int round(float value);
    QSize getNearestResolutionForSecondary(const QSize& resolution, float aspectRatio) const;
    int findClosestRateFloor(const std::vector<int>& values, int threshold) const;
    int  getH264StreamProfile(const VideoOptionsLocal& videoOptionsLocal);
    void checkMaxFps(VideoConfigsResp& response, const QString& encoderId);


    void updateVideoSource(VideoSource* source, const QRect& maxRect) const;
    CameraDiagnostics::Result sendVideoSourceToCamera(VideoSource* source);

    QRect getVideoSourceMaxSize(const QString& configToken);

    bool isH264Allowed() const; // block H264 if need for compatble with some onvif devices
    CameraDiagnostics::Result updateVEncoderUsage(QList<VideoOptionsLocal>& optionsList);
protected:
    std::unique_ptr<onvifXsd__EventCapabilities> m_eventCapabilities;
    QList<QSize> m_resolutionList; //Sorted desc
    QList<QSize> m_secondaryResolutionList;
    std::unique_ptr<OnvifCameraSettingsResp> m_onvifAdditionalSettings;

    mutable QMutex m_physicalParamsMutex;
    QDateTime m_advSettingsLastUpdated;

    virtual bool startInputPortMonitoringAsync( std::function<void(bool)>&& completionHandler ) override;
    virtual void stopInputPortMonitoringAsync() override;
    virtual bool isInputPortMonitored() const override;

    qreal getBestSecondaryCoeff(const QList<QSize> resList, qreal aspectRatio) const;
    int getSecondaryIndex(const QList<VideoOptionsLocal>& optList) const;
    //!Registeres local NotificationConsumer in resource's NotificationProducer
    bool registerNotificationConsumer();
    void updateFirmware();
private slots:
    void onRenewSubscriptionTimer( quint64 timerID );

private:
    // TODO: #Elric #enum
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

        QString propertyOperation;
        QDateTime utcTime;
        std::list<SimpleItem> source;
        SimpleItem data;

        NotificationMessageParseHandler();

        virtual bool startElement( const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& atts ) override;
        virtual bool endElement( const QString& namespaceURI, const QString& localName, const QString& qName ) override;

    private:
        // TODO: #Elric #enum
        enum State
        {
            init,
            readingMessage,
            readingSource,
            readingSourceItem,
            readingData,
            readingDataItem,
            //skipping unknown element
            skipping
        };

        std::stack<State> m_parseStateStack;
    };

    class TriggerOutputTask
    {
    public:
        QString outputID;
        bool active;
        unsigned int autoResetTimeoutMS;

        TriggerOutputTask()
        :
            active( false ),
            autoResetTimeoutMS( 0 )
        {
        }

        TriggerOutputTask(
            const QString _outputID,
            const bool _active,
            const unsigned int _autoResetTimeoutMS )
        :
            outputID( _outputID ),
            active( _active ),
            autoResetTimeoutMS( _autoResetTimeoutMS )
        {
        }
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
    QSize m_primaryResolution;
    QSize m_secondaryResolution;
    int m_primaryH264Profile;
    int m_secondaryH264Profile;
    int m_audioBitrate;
    int m_audioSamplerate;
    //QRect m_physicalWindowSize;
    QSize m_videoSourceSize;
    QString m_primaryVideoEncoderId;
    QString m_secondaryVideoEncoderId;
    QString m_audioEncoderId;
    QString m_videoSourceId;
    QString m_audioSourceId;
    QString m_videoSourceToken;

    QString m_imagingUrl;
    QString m_ptzUrl;
    QString m_ptzProfileToken;
    QString m_ptzConfigurationToken;
    int m_timeDrift;
    int m_prevSoapCallResult;
    std::vector<RelayOutputInfo> m_relayOutputInfo;
    std::map<QString, bool> m_relayInputStates;
    std::string m_deviceIOUrl;
    QString m_onvifNotificationSubscriptionID;
    mutable QMutex m_ioPortMutex;
    bool m_inputMonitored;
    EventMonitorType m_eventMonitorType;
    quint64 m_nextPullMessagesTimerID;
    quint64 m_renewSubscriptionTimerID;
    int m_maxChannels;
    std::map<quint64, TriggerOutputTask> m_triggerOutputTasks;
    
    QMutex m_streamConfMutex;
    QWaitCondition m_streamConfCond;
    int m_streamConfCounter;
    CameraDiagnostics::Result m_prevOnvifResultCode; 
    QString m_onvifNotificationSubscriptionReference;
    QElapsedTimer m_monotonicClock;
    qint64 m_prevPullMessageResponseClock;
    QSharedPointer<GSoapAsyncPullMessagesCallWrapper> m_asyncPullMessagesCallWrapper;

    bool createPullPointSubscription();
    void removePullPointSubscription();
    void pullMessages( quint64 timerID );
    void onPullMessagesDone(GSoapAsyncPullMessagesCallWrapper* asyncWrapper, int resultCode);
    void onPullMessagesResponseReceived(
        PullPointSubscriptionWrapper* soapWrapper, 
        int resultCode,
        const _onvifEvents__PullMessagesResponse& response);
        //!Reads relay output list from resource
    bool fetchRelayOutputs( std::vector<RelayOutputInfo>* const relayOutputs );
    bool fetchRelayOutputInfo( const std::string& outputID, RelayOutputInfo* const relayOutputInfo );
    bool fetchRelayInputInfo();
    bool fetchPtzInfo();
    bool setRelayOutputSettings( const RelayOutputInfo& relayOutputInfo );
    void checkPrimaryResolution(QSize& primaryResolution);
    void setRelayOutputStateNonSafe(
        quint64 timerID,
        const QString& outputID,
        bool active,
        unsigned int autoResetTimeoutMS );
    CameraDiagnostics::Result fetchAndSetDeviceInformationPriv( bool performSimpleCheck );
    QnAbstractPtzController* createSpecialPtzController();
    bool trustMaxFPS();
};

#endif //ENABLE_ONVIF

#endif //onvif_resource_h
