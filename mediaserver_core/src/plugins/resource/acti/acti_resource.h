#ifndef __ACTI_RESOURCE_H__
#define __ACTI_RESOURCE_H__

#ifdef ENABLE_ACTI
#include <QtCore/QFile>
#include <QtCore/QMap>
#include <nx/utils/thread/mutex.h>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "nx/streaming/media_data_packet.h"
#include "utils/common/request_param.h"
#include <nx/utils/timer_manager.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <utils/xml/camera_advanced_param_reader.h>
#include <network/multicodec_rtp_reader.h>



class QnActiPtzController;

class QnActiResource
:
    public QnPhysicalCameraResource,
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

    QnActiResource();
    ~QnActiResource();

    //!Implementation of QnNetworkResource::checkIfOnlineAsync
    virtual void checkIfOnlineAsync( std::function<void(bool)> completionHandler ) override;

    virtual QString getDriverName() const override;

    virtual bool getParamPhysical(const QString &id, QString &value) override;
    virtual bool getParamsPhysical(const QSet<QString> &idList, QnCameraAdvancedParamValueList& result) override;
    virtual bool setParamPhysical(const QString &id, const QString& value) override;
    virtual bool setParamsPhysical(const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result) override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;
    virtual bool hasDualStreaming() const override;
    virtual int getMaxFps() const override;

    QString getRtspUrl(int actiChannelNum) const; // in range 1..N

    /*!
        \param localAddress If not NULL, filled with local ip address, used to connect to camera
    */
    QByteArray makeActiRequest(const QString& group, const QString& command, CLHttpStatus& status, bool keepAllData = false, QString* const localAddress = NULL) const;
    QSize getResolution(Qn::ConnectionRole role) const;
    int roundFps(int srcFps, Qn::ConnectionRole role) const;
    int roundBitrate(int srcBitrateKbps) const;
    QString formatBitrateString(int bitrateKbps) const;

    QSet<QString> getAvailableEncoders() const;
    RtpTransport::Value getDesiredTransport() const;

    bool isAudioSupported() const;
    virtual QnAbstractPtzController *createPtzControllerInternal() override;

    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QnIOPortDataList getRelayOutputList() const override;
    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QnIOPortDataList getInputPortList() const override;
    //!Implementation of QnSecurityCamResource::setRelayOutputState
    /*!
        Actual request is performed asynchronously. This method only posts task to the queue
    */
    virtual bool setRelayOutputState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;
    //!Implementation of TimerEventHandler::onTimer
    virtual void onTimer( const quint64& timerID );

    static CLHttpStatus makeActiRequest(
        const QUrl& url,
        const QAuthenticator& auth,
        const QString& group,
        const QString& command,
        bool keepAllData,
        QByteArray* const msgBody,
        QString* const localAddress = nullptr );

    static QByteArray unquoteStr(const QByteArray& value);
    static ActiSystemInfo parseSystemInfo(const QByteArray& report);

    //!Called by http server on receiving message from camera
    void cameraMessageReceived( const QString& path, const QnRequestParamList& message );

    static void setEventPort(int eventPort);
    static int eventPort();

    virtual bool allowRtspVideoLayout() const override { return false; }
    bool SetupAudioInput();
protected:
    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    //!Implementation of QnSecurityCamResource::startInputPortMonitoringAsync
    virtual bool startInputPortMonitoringAsync( std::function<void(bool)>&& completionHandler ) override;
    //!Implementation of QnSecurityCamResource::stopInputPortMonitoringAsync
    virtual void stopInputPortMonitoringAsync() override;
    //!Implementation of QnSecurityCamResource::isInputPortMonitored
    virtual bool isInputPortMonitored() const override;

private:
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

    QMap<QString, QString> resolveQueries(QMap<QString, QnCameraAdvancedParamQueryInfo>& queries) const;

    void extractParamValues(const QString& paramValue, const QString& mask, QMap<QString, QString>& result) const;
    QString fillMissingParams(const QString& unresolvedTemplate, const QString& valueFromCamera) const;

    boost::optional<QString> tryToGetSystemInfoValue(const ActiSystemInfo& report, const QString& key) const;


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


    QSize m_resolution[MAX_STREAMS]; // index 0 for primary, index 1 for secondary
    QList<int> m_availFps[MAX_STREAMS];
    QMap<int, QString> m_availableBitrates;
    QSet<QString> m_availableEncoders;
    RtpTransport::Value m_desiredTransport;

    int m_rtspPort;
    bool m_hasAudio;
    QByteArray m_platform;
    QnMutex m_dioMutex;
    std::map<quint64, TriggerOutputTask> m_triggerOutputTasks;
    int m_outputCount;
    int m_inputCount;
    bool m_inputMonitored;
    QnMutex m_audioCfgMutex;
    QnMutex m_physicalParamsMutex;
    boost::optional<bool> m_audioInputOn;
    QnCameraAdvancedParams m_advancedParameters;
    QnCameraAdvancedParams m_advancedParametersCache;

};

#endif // #ifdef ENABLE_ACTI
#endif // __ACTI_RESOURCE_H__
