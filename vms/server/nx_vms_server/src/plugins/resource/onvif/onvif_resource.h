#pragma once

#ifdef ENABLE_ONVIF

#include <list>
#include <memory>
#include <stack>
#include <set>

#include <QString>
#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtCore/QTimeZone>
#include <QSharedPointer>
#include <nx/utils/thread/wait_condition.h>
#include <QtXml/QXmlDefaultHandler>
#include <QElapsedTimer>
#include <QtCore/QTimeZone>

#include <nx/kit/ini_config.h>

#include <nx/vms/server/resource/camera.h>
#include <core/resource/camera_advanced_param.h>

#include <core/resource/resource_data_structures.h>
#include <nx/vms/server/resource/camera_advanced_parameters_providers.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/streaming/media_data_packet.h>
#include <onvif/soapStub.h>

#include <plugins/resource/onvif/onvif_multicast_parameters_provider.h>

#include "soap_wrapper.h"
#include "video_encoder_config_options.h"

struct SoapTimeouts;
class onvifXsd__AudioEncoderConfigurationOption;
class onvifXsd__VideoSourceConfigurationOptions;
class onvifXsd__VideoEncoderConfigurationOptions;
class onvifXsd__VideoEncoderConfiguration;
class oasisWsnB2__NotificationMessageHolderType;
class onvifXsd__VideoSourceConfiguration;
class onvifXsd__EventCapabilities;

class onvifXsd__VideoEncoder2ConfigurationOptions;
class _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse;

typedef onvifXsd__AudioEncoderConfigurationOption AudioOptions;
typedef onvifXsd__VideoSourceConfigurationOptions VideoSrcOptions;
typedef onvifXsd__VideoEncoderConfigurationOptions VideoOptions;
typedef onvifXsd__VideoEncoderConfiguration VideoEncoder;

//first = width, second = height

class QDomElement;
class QnOnvifImagingProxy;
class QnOnvifMaintenanceProxy;

template<typename SyncWrapper, typename Request, typename Response>
class GSoapAsyncCallWrapper;

/**
 * This structure is used during discovery process.
 * These data are read by getDeviceInformation request and may override data from multicast packet.
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

struct QnOnvifServiceUrls
{
    QString deviceServiceUrl; //< specially used
    QString mediaServiceUrl;
    QString media2ServiceUrl;
    QString ptzServiceUrl;
    QString imagingServiceUrl;
    QString deviceioServiceUrl;

    // Currently not used, m_eventsCapabilities->XAddr used instead.
    // TODO: fix it: use eventsServiceUrl.
    //QString eventsServiceUrl;

    QString getUrl(OnvifWebService onvifWebService) const;
};

struct OnvifIniConfig: public nx::kit::IniConfig
{
    OnvifIniConfig(): IniConfig("vms_server_onvif.ini") {}

    static OnvifIniConfig& instance()
    {
        static OnvifIniConfig ini;
        return ini;
    }
};

class QnPlOnvifResource:
    public nx::vms::server::resource::Camera
{
    Q_OBJECT
    using base_type = nx::vms::server::resource::Camera;

public:
    using GSoapAsyncPullMessagesCallWrapper = GSoapAsyncCallWrapper<
        PullPointSubscriptionWrapper,
        _onvifEvents__PullMessages, _onvifEvents__PullMessagesResponse>;

    struct RelayOutputInfo
    {
        std::string token;
        bool isBistable = false;
        // The time (in milliseconds) after which the relay returns to its idle state, if it is in
        // monostable mode; in bistable mode the value of the parameter shall be ignored.
        LONG64 delayTimeMs = 0;
        bool activeByDefault = false;

        RelayOutputInfo() = default;
        RelayOutputInfo(std::string _token, bool _isBistable,
            LONG64 _delayTimeMs, bool _activeByDefault)
            :
            token(std::move(_token)), isBistable(_isBistable),
            delayTimeMs(_delayTimeMs), activeByDefault(_activeByDefault)
        {
        }
    };

    struct RelayInputState
    {
        bool value;
        qint64 timestamp;

        RelayInputState(): value(false), timestamp(0) {}
    };

    enum AUDIO_CODEC
    {
        AUDIO_NONE,
        G726,
        G711,
        AAC,
        AMR,
        SIZE_OF_AUDIO_CODECS // TODO: #Elric #enum
    };

    class VideoEncoderCapabilities
    {
    public:
        VideoEncoderCapabilities() = default;

        VideoEncoderCapabilities(std::string videoEncoderToken,
            const onvifXsd__VideoEncoderConfigurationOptions& options,
            QnBounds frameRateBounds = QnBounds());

        VideoEncoderCapabilities(std::string videoEncoderToken,
            const onvifXsd__VideoEncoder2ConfigurationOptions& resp,
            QnBounds frameRateBounds = QnBounds());

        static std::vector<QnPlOnvifResource::VideoEncoderCapabilities> createVideoEncoderCapabilitiesList(
            const std::string& videoEncoderToken,
            const onvifXsd__VideoEncoderConfigurationOptions& options,
            QnBounds frameRateBounds);

        std::string videoEncoderToken;
        SupportedVideoEncoding encoding = SupportedVideoEncoding::NONE;

        // Profiles for h264 codec. May be read by Media1 (from onvifXsd__H264Profile)
        // or by Media2 (from onvifXsd__VideoEncodingProfiles).
        QVector<onvifXsd__H264Profile> h264Profiles; //< filled for h264 codec

        // Profiles for h265 codec. May be read only by Media2.
        // Only two values are appropriate: Main and Main10.
        // Usually is empty (this obviously means, Main is used).
        QVector<onvifXsd__VideoEncodingProfiles> h265Profiles; //< filled for h265 codec

        QList<QSize> resolutions;
        int minQ = -1;
        int maxQ = 1;
        int frameRateMin = -1;
        int frameRateMax = -1;
        int govMin = -1;
        int govMax = -1;
        bool isUsedInProfiles = false;
        QString currentProfile;

    private:
        int restrictFrameRate(int frameRate, QnBounds frameRateBounds) const;
    };

    static const QString MANUFACTURE;
    static const float QUALITY_COEF;
    static const int MAX_AUDIO_BITRATE;
    static const int MAX_AUDIO_SAMPLERATE;
    // Time, during which adv settings will not be obtained from camera (in milliseconds)
    static const int ADVANCED_SETTINGS_VALID_TIME;

    static const QString fetchMacAddress(
        const NetIfacesResp& response, const QString& senderIpAddress);

    QnPlOnvifResource(QnMediaServerModule* serverModule);
    virtual ~QnPlOnvifResource();

    static const QString createOnvifEndpointUrl(const QString& ipAddress);

    virtual void setHostAddress(const QString &ip) override;

    //!Implementation of QnNetworkResource::checkIfOnlineAsync
    virtual void checkIfOnlineAsync(std::function<void(bool)> completionHandler) override;

    virtual QString getDriverName() const override;

    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source) override;

    virtual int getMaxOnvifRequestTries() const { return 1; }

    /*!
        Actual request is performed asynchronously. This method only posts task to the queue
    */
    virtual bool setOutputPortState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS) override;

    int innerQualityToOnvif(Qn::StreamQuality quality, int minQuality, int maxQuality) const;
    const QString createOnvifEndpointUrl() const
    {
        return createOnvifEndpointUrl(getHostAddress());
    }

    int getAudioBitrate() const;
    int getAudioSamplerate() const;
    int getTimeDrift() const; // return clock diff between camera and local clock at seconds
    void setTimeDrift(int value); // return clock diff between camera and local clock at seconds
    //bool isSoapAuthorized() const;
    const QSize getVideoSourceSize() const;

    void updateOnvifUrls(const QnPlOnvifResourcePtr& other);

    QString getMediaUrl() const;
    void setMediaUrl(const QString& src);

    QString getMedia2Url() const;
    void setMedia2Url(const QString& src);

    QString getImagingUrl() const;
    void setImagingUrl(const QString& src);

    QString getDeviceIOUrl() const;
    void setDeviceIOUrl(const QString& src);

    QString getDeviceOnvifUrl() const;
    void setDeviceOnvifUrl(const QString& src);

    QString getPtzUrl() const;
    void setPtzUrl(const QString& src);

    std::string videoSourceConfigurationToken() const;
    void setVideoSourceConfigurationToken(std::string token);

    std::string videoSourceToken() const;
    void setVideoSourceToken(std::string token);

    std::string videoEncoderConfigurationToken(nx::vms::api::StreamIndex streamIndex) const;
    void setVideoEncoderConfigurationToken(
        nx::vms::api::StreamIndex streamIndex,
        std::string token);

    std::string audioSourceConfigurationToken() const;
    void setAudioSourceConfigurationToken(std::string token);

    std::string audioSourceToken() const;
    void setAudioSourceToken(std::string token);

    std::string audioEncoderConfigurationToken() const;
    void setAudioEncoderConfigurationToken(std::string token);

    std::string audioOutputConfigurationToken() const;
    void setAudioOutputConfigurationToken(std::string value);

    std::string ptzConfigurationToken() const;
    void setPtzConfigurationToken(std::string token);

    std::string ptzProfileToken() const;
    void setPtzProfileToken(std::string token);

    AUDIO_CODEC getAudioCodec() const;

    SoapParams makeSoapParams(OnvifWebService onvifWebService, bool tcpKeepAlive = false) const;
    SoapParams makeSoapParams(std::string endpoint, bool tcpKeepAlive) const;

    virtual void setOnvifRequestsRecieveTimeout(int timeout);
    virtual void setOnvifRequestsSendTimeout(int timeout);

    virtual int getOnvifRequestsRecieveTimeout() const;
    virtual int getOnvifRequestsSendTimeout() const;

    /** calculate clock diff between camera and local clock at seconds. */
    void calcTimeDrift(int* outSoapRes = nullptr) const;
    static int calcTimeDrift(
        const SoapTimeouts& timeouts, const QString& deviceUrl,
        int* outSoapRes = nullptr, QTimeZone* timeZone = nullptr);

    virtual QnCameraAdvancedParamValueMap getApiParameters(const QSet<QString>& ids);
    virtual QSet<QString> setApiParameters(const QnCameraAdvancedParamValueMap& values);

    static CameraDiagnostics::Result readDeviceInformation(
        QnCommonModule* commonModule,
        const SoapTimeouts& onvifTimeouts,
        const QString& onvifUrl,
        const QAuthenticator& auth, int timeDrift, OnvifResExtInfo* extInfo);
    CameraDiagnostics::Result readDeviceInformation();
    CameraDiagnostics::Result getFullUrlInfo();

    //!Relay input with token \a relayToken has changed its state to \a active
    //void notificationReceived(const std::string& relayToken, bool active);

    /** Notifications with timestamp earlier than \a minNotificationTime are ignored. */
    void handleOneNotification(
        const oasisWsnB2__NotificationMessageHolderType& notification,
        time_t minNotificationTime = (time_t)-1);

    void onRelayInputStateChange(const QString& name, const RelayInputState& state);
    QString fromOnvifDiscoveredUrl(const std::string& onvifUrl, bool updatePort = true);

    virtual int getMaxChannelsFromDriver() const override;

    void updateToChannel(int value);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* dataProvider) const override;

    virtual CameraDiagnostics::Result fetchChannelCount(bool limitedByEncoders = true);

    virtual CameraDiagnostics::Result sendVideoEncoderToCameraEx(
        onvifXsd__VideoEncoderConfiguration& encoder,
        StreamIndex streamIndex,
        const QnLiveStreamParams& params);

    virtual CameraDiagnostics::Result sendVideoEncoder2ToCameraEx(
        onvifXsd__VideoEncoder2Configuration& encoder,
        StreamIndex streamIndex,
        const QnLiveStreamParams& params);

    virtual int suggestBitrateKbps(
        const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const override;

    QnMutex* getStreamConfMutex();
    virtual void beforeConfigureStream(Qn::ConnectionRole role);
    virtual void afterConfigureStream(Qn::ConnectionRole role);
    virtual CameraDiagnostics::Result customStreamConfiguration(
        Qn::ConnectionRole role);

    double getClosestAvailableFps(double desiredFps);

    static bool isCameraForcedToOnvif(
        QnResourceDataPool* dataPool,
        const QString& manufacturer, const QString& model);

    VideoEncoderCapabilities primaryVideoCapabilities() const;
    VideoEncoderCapabilities secondaryVideoCapabilities() const;

    void updateVideoEncoder1(
        onvifXsd__VideoEncoderConfiguration& encoder,
        StreamIndex streamIndex,
        const QnLiveStreamParams& streamParams);

    void updateVideoEncoder2(
        onvifXsd__VideoEncoder2Configuration& encoder,
        StreamIndex streamIndex,
        const QnLiveStreamParams& streamParams);

    SoapTimeouts onvifTimeouts() const;

signals:
    void advancedParameterChanged(const QString &id, const QString &value);

protected:
    virtual QnAbstractPtzController* createPtzControllerInternal() const override;
    int strictBitrate(int bitrate, Qn::ConnectionRole role) const;
    void setAudioCodec(AUDIO_CODEC c);

    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDriver(
        StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual CameraDiagnostics::Result initOnvifCapabilitiesAndUrls(
        DeviceSoapWrapper& deviceSoapWrapper,
        _onvifDevice__GetCapabilitiesResponse* outCapabilitiesResponse);
    virtual CameraDiagnostics::Result initializeMedia(
        const _onvifDevice__GetCapabilitiesResponse& onvifCapabilities);
    virtual CameraDiagnostics::Result initializePtz(
        const _onvifDevice__GetCapabilitiesResponse& onvifCapabilities);
    virtual CameraDiagnostics::Result initializeIo(
        const _onvifDevice__GetCapabilitiesResponse& onvifCapabilities);
    virtual CameraDiagnostics::Result initializeAdvancedParameters(
        const _onvifDevice__GetCapabilitiesResponse& onvifCapabilities);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    /**
     * Filter out resolutions, that are greater then video source resolution.
     * @note: As this function is virtual, descendents may change any resource capabilities.
     */
    virtual CameraDiagnostics::Result updateResourceCapabilities();

    virtual bool loadAdvancedParametersTemplate(QnCameraAdvancedParams &params) const;
    virtual void initAdvancedParametersProvidersUnderLock(QnCameraAdvancedParams &params);
    virtual QSet<QString> calculateSupportedAdvancedParameters() const;
    virtual void fetchAndSetAdvancedParameters();

    virtual bool loadAdvancedParamsUnderLock(QnCameraAdvancedParamValueMap &values);
    virtual bool setAdvancedParameterUnderLock(
        const QnCameraAdvancedParameter &parameter, const QString &value);
    virtual bool setAdvancedParametersUnderLock(
        const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result);

    virtual CameraDiagnostics::Result customInitialization(
        const _onvifDevice__GetCapabilitiesResponse& /*capabilitiesResponse*/)
    {
        return CameraDiagnostics::NoErrorResult();
    }

    void setPrimaryVideoCapabilities(const VideoEncoderCapabilities& capabilities)
    {
        if (!m_primaryStreamCapabilitiesList.empty())
            m_primaryStreamCapabilitiesList.front() = capabilities;
    }
    void setSecondaryVideoCapabilities(const VideoEncoderCapabilities& capabilities)
    {
        if (!m_secondaryStreamCapabilitiesList.empty())
            m_secondaryStreamCapabilitiesList.front() = capabilities;
    }

    boost::optional<onvifXsd__H264Profile> getH264StreamProfile(
        const VideoEncoderCapabilities& videoEncoderCapabilities);

    CameraDiagnostics::Result sendVideoEncoderToCamera(
        onvifXsd__VideoEncoderConfiguration& encoderConfig);
    CameraDiagnostics::Result sendVideoEncoder2ToCamera(
        onvifXsd__VideoEncoder2Configuration& encoderConfig);

    CameraDiagnostics::Result fetchAndSetVideoResourceOptions();
    CameraDiagnostics::Result fetchAndSetAudioResourceOptions();

    CameraDiagnostics::Result fetchAndSetVideoSource();
    CameraDiagnostics::Result fetchAndSetAudioSource();

private:
    QnIOPortDataList generateOutputPorts() const;

    CameraDiagnostics::Result ReadVideoEncoderOptionsForToken(
        std::string token, QList<VideoEncoderCapabilities>* dstOptionsList,
        const QnBounds& frameRateBounds);
    CameraDiagnostics::Result fetchAndSetVideoEncoderOptions();
    bool fetchAndSetAudioEncoderOptions(MediaSoapWrapper& soapWrapper);
    bool fetchAndSetDualStreaming();
    bool fetchAndSetAudioEncoder(MediaSoapWrapper& soapWrapper);

    void setAudioEncoderOptions(const AudioOptions& options);

    int findClosestRateFloor(const std::vector<int>& values, int threshold) const;
    void checkMaxFps(onvifXsd__VideoEncoderConfiguration* configuration);

    void updateVideoSource(onvifXsd__VideoSourceConfiguration* source, const QRect& maxRect) const;
    CameraDiagnostics::Result sendVideoSourceToCamera(onvifXsd__VideoSourceConfiguration* source);

    QRect getVideoSourceMaxSize(std::string token);

    CameraDiagnostics::Result updateVideoEncoderUsage(QList<VideoEncoderCapabilities>& optionsList);

    bool checkResultAndSetStatus(const CameraDiagnostics::Result& result);

    std::set<QString> notificationTopicsForMonitoring() const;
    std::set<QString> allowedInputSourceNames() const;

    /** fill m_primaryStreamCapabilitiesList and m_secondaryStreamCapabilitiesList */
    void fillStreamCapabilityLists(const QList<VideoEncoderCapabilities>& capabilitiesList);

    VideoEncoderCapabilities findVideoEncoderCapabilities(
        SupportedVideoEncoding encoding, StreamIndex streamIndex);

protected:
    std::unique_ptr<onvifXsd__EventCapabilities> m_eventCapabilities;

    std::vector<VideoEncoderCapabilities> m_primaryStreamCapabilitiesList;
    std::vector<VideoEncoderCapabilities> m_secondaryStreamCapabilitiesList;

    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;

    qreal getBestSecondaryCoeff(const QList<QSize> resList, qreal aspectRatio) const;
    int getSecondaryIndex(const QList<VideoEncoderCapabilities>& optList) const;
    //!Registers local NotificationConsumer in resource's NotificationProducer
    bool registerNotificationConsumer();
    void updateFirmware();
    void scheduleRetrySubscriptionTimer();
    virtual bool subscribeToCameraNotifications();

    bool createPullPointSubscription();
    bool loadXmlParametersInternal(
        QnCameraAdvancedParams &params, const QString& paramsTemplateFileName) const;
    void setMaxChannels(int value);

    virtual std::vector<Camera::AdvancedParametersProvider*> advancedParametersProviders() override;
    virtual QnAudioTransmitterPtr initializeTwoWayAudio();

private slots:
    void onRenewSubscriptionTimer(quint64 timerID);

private:
    // TODO: #Elric #enum
    enum EventMonitorType
    {
        emtNone,
        emtNotification,
        emtPullPoint
    };

    //!Parses <dom0:SubscriptionId xmlns:dom0=\"(null)\">1</dom0:SubscriptionId>
    class SubscriptionReferenceParametersParseHandler: public QXmlDefaultHandler
    {
    public:
        QString subscriptionID;

        SubscriptionReferenceParametersParseHandler();

        virtual bool characters(const QString& ch) override;
        virtual bool startElement(const QString& namespaceURI, const QString& localName,
            const QString& qName, const QXmlAttributes& atts) override;
        virtual bool endElement(const QString& namespaceURI, const QString& localName,
            const QString& qName) override;

    private:
        bool m_readingSubscriptionID;
    };

    struct onvifSimpleItem
    {
        std::string name;
        std::string value;
        onvifSimpleItem() = default;
        onvifSimpleItem(const char* name, const char* value):
            name(name ? name : ""), value(value ? value : "")
        {
        }
    };

    // TODO: The following static functions should be moved to a separate class in 4.1.
    static const char* attributeTextByName(const soap_dom_element& element, const char* attributeName);
    static onvifSimpleItem parseSimpleItem(const soap_dom_element& element);

    static onvifSimpleItem parseChildSimpleItem(const soap_dom_element& element);
    static std::vector<onvifSimpleItem> parseChildSimpleItems(const soap_dom_element& element);
    static void parseSourceAndData(const soap_dom_element& element,
        std::vector<onvifSimpleItem>* outSource, onvifSimpleItem* outData);

    static QString parseEventTopic(const char* text);
    static QDateTime parseDateTime(const soap_dom_attribute* att, QTimeZone timeZone);
    static std::string makeItemNameList(const std::vector<onvifSimpleItem>& items);

    struct TriggerOutputTask
    {
        QString outputID;
        bool active;
        unsigned int autoResetTimeoutMS;

        TriggerOutputTask(): active(false), autoResetTimeoutMS(0) {}

        TriggerOutputTask(
            const QString outputID, const bool active, const unsigned int autoResetTimeoutMS)
            :
            outputID(outputID),
            active(active),
            autoResetTimeoutMS(autoResetTimeoutMS)
        {
        }
    };

    static const char* ONVIF_PROTOCOL_PREFIX;
    static const char* ONVIF_URL_SUFFIX;
    static const int DEFAULT_IFRAME_DISTANCE;

    QMap<int, QRect> m_motionWindows;
    QMap<int, QRect> m_motionMask;

    AUDIO_CODEC m_audioCodec;
    int m_audioBitrate;
    int m_audioSamplerate;
    QSize m_videoSourceSize;

    std::string m_videoSourceConfigurationToken;
    std::string m_videoSourceToken;
    std::string m_audioSourceConfigurationToken;
    std::string m_audioSourceToken;
    std::string m_audioEncoderConfigurationToken;
    std::string m_audioOutputConfigurationToken;
    std::string m_ptzConfigurationToken;
    std::string m_ptzProfileToken;

    std::map<nx::vms::api::StreamIndex, std::string> m_videoEncoderConfigurationTokens;

    QString m_imagingUrl;
    QString m_ptzUrl;
    mutable int m_timeDrift;
    mutable QElapsedTimer m_timeDriftTimer;
    mutable QTimeZone m_cameraTimeZone;
    std::vector<RelayOutputInfo> m_relayOutputInfo;
    bool m_isRelayOutputInversed;
    bool m_fixWrongInputPortNumber;
    bool m_fixWrongOutputPortToken;
    std::map<QString, RelayInputState> m_relayInputStates;
    QString m_onvifNotificationSubscriptionID;
    mutable QnMutex m_ioPortMutex;
    bool m_inputMonitored;
    qint64 m_clearInputsTimeoutUSec;
    EventMonitorType m_eventMonitorType;
    quint64 m_nextPullMessagesTimerID;
    quint64 m_renewSubscriptionTimerID;
    int m_maxChannels;
    std::map<quint64, TriggerOutputTask> m_triggerOutputTasks;

    QnMutex m_streamConfMutex;
    QnWaitCondition m_streamConfCond;
    int m_streamConfCounter;
    CameraDiagnostics::Result m_prevOnvifResultCode;
    QString m_onvifNotificationSubscriptionReference;

    QElapsedTimer m_pullMessagesResponseElapsedTimer;
    QSharedPointer<GSoapAsyncPullMessagesCallWrapper> m_asyncPullMessagesCallWrapper;

    QString m_portNamePrefixToIgnore;
    size_t m_inputPortCount;
    std::vector<QString> m_portAliases;
    std::unique_ptr<onvifXsd__H264Configuration> m_tmpH264Conf;

    std::unique_ptr<int> m_govLength;
    std::unique_ptr<std::string> m_profile;

    void removePullPointSubscription();
    void pullMessages(quint64 timerID);
    void onPullMessagesDone(GSoapAsyncPullMessagesCallWrapper* asyncWrapper, int resultCode);
    /**
     * Used for cameras that do not support renew request.
     */
    void scheduleRenewSubscriptionTimer(unsigned int timeoutSec);
    void renewPullPointSubscriptionFallback(quint64 timerId);

    /** Handle all notifications listed in the response. */
    void handleAllNotifications(const _onvifEvents__PullMessagesResponse& response);

    //!Reads relay output list from resource
    bool fetchRelayOutputs(std::vector<RelayOutputInfo>* relayOutputInfoList);
    bool fetchRelayOutputInfo(const std::string& outputID, RelayOutputInfo* const relayOutputInfo);
    bool fetchRelayInputInfo(const _onvifDevice__GetCapabilitiesResponse& capabilitiesResponse);
    bool fetchPtzInfo();
    bool setRelayOutputInfo(const RelayOutputInfo& relayOutputInfo);
    void checkPrimaryResolution(QSize& primaryResolution);
    void setOutputPortStateNonSafe(
        quint64 timerID,
        const QString& outputID,
        bool active,
        unsigned int autoResetTimeoutMS);
    QnAbstractPtzController* createSpecialPtzController() const;
    bool trustMaxFPS();
    CameraDiagnostics::Result fetchOnvifCapabilities(
        DeviceSoapWrapper& soapWrapper,
        _onvifDevice__GetCapabilitiesResponse* response);
    CameraDiagnostics::Result fetchOnvifMedia2Url(QString* url);
    void fillFullUrlInfo(const _onvifDevice__GetCapabilitiesResponse& response);
    void detectCapabilities(const _onvifDevice__GetCapabilitiesResponse& response);
    bool getVideoEncoderTokens(BaseSoapWrapper& soapWrapper,
        const std::vector<onvifXsd__VideoEncoderConfiguration*>& configurations,
        QStringList* tokenList);
    QString getInputPortNumberFromString(const QString& portName);
    QnAudioTransmitterPtr initializeTwoWayAudioByResourceData();

protected:
    // Logging functions
    //** SOAP request failed. */
    QString makeSoapFailMessage(BaseSoapWrapper& soapWrapper,
        const QString& caller, const QString& requestCommand,
        int soapError, const QString& text = QString()) const;

    //** SOAP request succeeded. */
    QString makeSoapSuccessMessage(BaseSoapWrapper& soapWrapper,
        const QString& caller, const QString& requestCommand,
        const QString& text = QString()) const;

    //** SOAP response has no desired parameter. */
    QString makeSoapNoParameterMessage(BaseSoapWrapper& soapWrapper,
        const QString& missedParameter, const QString& caller, const QString& requestCommand,
        const QString& text = QString()) const;

    //** SOAP response has no desired parameter. */
    QString makeSoapNoRangeParameterMessage(BaseSoapWrapper& soapWrapper,
        const QString& rangeParameter, int index, const QString& caller,
        const QString& requestCommand, const QString& text = QString()) const;

    //** SOAP response has a range (e.g. vector) of parameters of no enough size. */
    QString makeSoapSmallRangeMessage(BaseSoapWrapper& soapWrapper,
        const QString& rangeParameter, int rangeSize, int desiredSize,
        const QString& caller, const QString& requestCommand,
        const QString& text = QString()) const;

    //* SOAP request failed - static analogue for makeSoapFailMessage. */
    static QString makeStaticSoapFailMessage(BaseSoapWrapper& soapWrapper,
        const QString& requestCommand, int soapError, const QString& text = QString());

    //** SOAP response is incomplete - static analogue for makeSoapNoParameterMessage. */
    static QString makeStaticSoapNoParameterMessage(BaseSoapWrapper& soapWrapper,
        const QString& missedParameter, const QString& caller, const QString& requestCommand,
        const QString& text = QString());

    QString makeFailMessage(const QString& text) const;
    void updateTimer(
        nx::utils::TimerId* timerId, std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void(nx::utils::TimerId)> function);
private:
    mutable QnMutex m_physicalParamsMutex;
    std::unique_ptr<QnOnvifImagingProxy> m_imagingParamsProxy;
    std::unique_ptr<QnOnvifMaintenanceProxy> m_maintenanceProxy;
    QElapsedTimer m_advSettingsLastUpdated;
    QnCameraAdvancedParamValueMap m_advancedParamsCache;
    mutable QnConstResourceVideoLayoutPtr m_videoLayout;
    mutable QnOnvifServiceUrls m_serviceUrls;
    nx::utils::AsyncOperationGuard m_asyncConnectGuard;
protected:
    nx::vms::server::resource::ApiMultiAdvancedParametersProvider<QnPlOnvifResource> m_advancedParametersProvider;
    nx::vms::server::resource::OnvifMulticastParametersProvider m_primaryMulticastParametersProvider;
    nx::vms::server::resource::OnvifMulticastParametersProvider m_secondaryMulticastParametersProvider;
    int m_onvifRecieveTimeout;
    int m_onvifSendTimeout;
};

#endif //ENABLE_ONVIF
