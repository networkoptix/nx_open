#pragma once

#ifdef ENABLE_ACTI

#include <QtCore/QFile>
#include <QtCore/QMap>

#include <network/multicodec_rtp_reader.h>
#include <nx/vms/server/resource/camera_advanced_parameters_providers.h>
#include <nx/vms/server/resource/camera.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <utils/common/request_param.h>
#include <utils/xml/camera_advanced_param_reader.h>

class QnActiPtzController;

class QnActiResource:
    public nx::vms::server::resource::Camera,
    public nx::utils::TimerEventHandler
{
    Q_OBJECT

public:
    typedef QMap<QString, QString> ActiSystemInfo;

    static const QString MANUFACTURE;
    static const QString CAMERA_PARAMETER_GROUP_ENCODER;
    static const QString CAMERA_PARAMETER_GROUP_SYSTEM;
    static const QString CAMERA_PARAMETER_GROUP_DEFAULT;
    static const QString DEFAULT_ADVANCED_PARAMETERS_TEMPLATE;
    static const QString ADVANCED_PARAMETERS_TEMPLATE_PARAMETER_NAME;

    static const int MAX_STREAMS = 2;

    QnActiResource(QnMediaServerModule* serverModule);
    ~QnActiResource() override;

    //!Implementation of QnNetworkResource::checkIfOnlineAsync
    virtual void checkIfOnlineAsync( std::function<void(bool)> completionHandler ) override;

    virtual QString getDriverName() const override;

    QnCameraAdvancedParamValueMap getApiParameters(const QSet<QString>& ids);
    QSet<QString> setApiParameters(const QnCameraAdvancedParamValueMap& values);

    virtual bool hasDualStreamingInternal() const override;

    QString getRtspUrl(int actiChannelNum) const; // in range 1..N

    int roundFps(int srcFps, Qn::ConnectionRole role) const;
    int roundBitrate(int srcBitrateKbps) const;
    QString formatBitrateString(int bitrateKbps) const;

    RtspTransport getDesiredTransport() const;

    bool isAudioSupported() const;

    /** Actual request is performed asynchronously. This method only posts task to the queue*/
    virtual bool setOutputPortState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;

    virtual void onTimer( const quint64& timerID ) override;

    /*!
        \param localAddress If not NULL, filled with local ip address, used to connect to camera
    */
    QByteArray makeActiRequest(
        const QString& group,
        const QString& command,
        nx::network::http::StatusCode::Value& status,
        bool keepAllData = false,
        QString* const localAddress = nullptr) const;

    static nx::network::http::StatusCode::Value makeActiRequestByUrl(
        const nx::utils::Url& url,
        bool keepAllData,
        QByteArray* const msgBody,
        QString* const localAddress = nullptr);

    static QByteArray unquoteStr(const QByteArray& value);
    static ActiSystemInfo parseSystemInfo(const QByteArray& report);

    //!Called by http server on receiving message from camera
    void cameraMessageReceived( const QString& path, const QnRequestParamList& message );

    static void setEventPort(int eventPort);
    static int eventPort();

    virtual bool allowRtspVideoLayout() const override { return false; }
    bool SetupAudioInput();

    static QString toActiEncoderString(const QString& value);
    static QString formatResolutionStr(const QSize& resolution);
protected:
    virtual QnAbstractPtzController* createPtzControllerInternal() const override;
    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDriver(
        StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;

private:
    struct CameraAdvancedParamQueryInfo
    {
        QString group;
        QString cmd;
    };

    nx::utils::Url createRequestUrl(const QAuthenticator& auth, const QString& group,
        const QString& command) const;

    QSize extractResolution(const QByteArray& resolutionStr) const;
    QList<QSize> parseResolutionStr(const QByteArray& resolutions);
    QMap<int, QString> parseVideoBitrateCap(const QByteArray& bitrateCap) const;
    QString bitrateToDefaultString(int bitrateKbps) const;

    void initialize2WayAudio( const ActiSystemInfo& systemInfo );
    void initializeIO( const ActiSystemInfo& systemInfo );
    bool isRtspAudioSupported(const QByteArray& platform, const QByteArray& firmware) const;
    void fetchAndSetAdvancedParameters();
    QString getAdvancedParametersTemplate() const;
    bool loadAdvancedParametersTemplateFromFile(QnCameraAdvancedParams& params, const QString& filename);
    QSet<QString> calculateSupportedAdvancedParameters(const QnCameraAdvancedParams& allParams) const;
    bool parseParameter(const QString& paramId, QnCameraAdvancedParamValue& param) const;
    void parseCameraParametersResponse(const QByteArray& response, QnCameraAdvancedParamValueList& result) const;
    void parseCameraParametersResponse(const QByteArray& response, QMap<QString, QString>& result) const;

    QString convertParamValueToDeviceFormat(const QString& paramValue, const QnCameraAdvancedParameter& param) const;
    QString convertParamValueFromDeviceFormat(const QString& paramValue, const QnCameraAdvancedParameter& param) const;
    QString getParamGroup(const QnCameraAdvancedParameter& param) const;
    QString getParamCmd(const QnCameraAdvancedParameter& param) const;

    QList<QnCameraAdvancedParameter> getParamsByIds(const QSet<QString>& idList) const;
    QMap<QString, QnCameraAdvancedParameter> getParamsMap(const QSet<QString>& idList) const;

    bool isMaintenanceParam(const QnCameraAdvancedParameter& param) const;

    QMap<QString, QString> buildGetParamsQueries(const QList<QnCameraAdvancedParameter>& params) const;
    QMap<QString, QString> buildSetParamsQueries(const QnCameraAdvancedParamValueList& values) const;
    QMap<QString, QString> buildMaintenanceQueries(const QnCameraAdvancedParamValueList& values) const;

    QMap<QString, QString> executeParamsQueries(const QMap<QString, QString>& queries, bool& isSuccessful) const;
    void parseParamsQueriesResult(
        const QMap<QString, QString>& queriesResult,
        const QList<QnCameraAdvancedParameter>& params,
        QnCameraAdvancedParamValueList& result) const;

    QMap<QString, QString> resolveQueries(QMap<QString, CameraAdvancedParamQueryInfo>& queries) const;

    void extractParamValues(const QString& paramValue, const QString& mask, QMap<QString, QString>& result) const;
    QString fillMissingParams(const QString& unresolvedTemplate, const QString& valueFromCamera) const;

    boost::optional<QString> tryToGetSystemInfoValue(const ActiSystemInfo& report, const QString& key) const;

    virtual std::vector<Camera::AdvancedParametersProvider*> advancedParametersProviders() override;
    CameraDiagnostics::Result maxFpsForSecondaryResolution(
        const QString& secondaryCodec, const QSize& secondaryResolution, int* outFps);
    QString bestPrimaryCodec() const;
    CameraDiagnostics::Result detectMaxFpsForSecondaryCodec();
private:
    class TriggerOutputTask
    {
    public:
        //!Starts with 1. 0 - invalid
        int outputID;
        bool active;
        unsigned int autoResetTimeoutMS;

        TriggerOutputTask()
        :
            outputID( 0 ),
            active( false ),
            autoResetTimeoutMS( 0 )
        {
        }

        TriggerOutputTask(
            const int _outputID,
            const bool _active,
            const unsigned int _autoResetTimeoutMS )
        :
            outputID( _outputID ),
            active( _active ),
            autoResetTimeoutMS( _autoResetTimeoutMS )
        {
        }
    };

    class ActiResponse
    {
    public:
        //!map<parameter name, param fields including empty ones>
        std::map<QByteArray, std::vector<QByteArray> > params;
    };

    struct ComparableSize: public QSize
    {
    public:
        ComparableSize(const QSize& value = QSize()): QSize(value) {}
        bool operator<(const QSize& other) const
        {
            if (width() != other.width())
                return width() < other.width();
            return height() < other.height();
        }
    };

    QList<QSize> m_resolutionList[MAX_STREAMS];
    QList<int> m_availFps[MAX_STREAMS];
    QMap<int, QString> m_availableBitrates;
    QMap<ComparableSize, int> m_maxSecondaryFps;
    QSet<QString> m_availableEncoders;
    RtspTransport m_desiredTransport;

    int m_rtspPort;
    bool m_hasAudio;
    QByteArray m_platform;
    QnMutex m_dioMutex;
    std::map<quint64, TriggerOutputTask> m_triggerOutputTasks;
    int m_outputCount;
    int m_inputCount;
    bool m_inputMonitored;
    QnMutex m_audioCfgMutex;
    boost::optional<bool> m_audioInputOn;
    nx::vms::server::resource::ApiMultiAdvancedParametersProvider<QnActiResource> m_advancedParametersProvider;
};

#endif // #ifdef ENABLE_ACTI
