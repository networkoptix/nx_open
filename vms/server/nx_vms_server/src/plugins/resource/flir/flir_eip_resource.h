#pragma once

#ifdef ENABLE_FLIR

#include <nx/vms/server/resource/camera_advanced_parameters_providers.h>
#include <nx/vms/server/resource/camera.h>
#include <nx/utils/timer_manager.h>
#include <utils/xml/camera_advanced_param_reader.h>

#include "eip_async_client.h"
#include "flir_eip_data.h"
#include "simple_eip_client.h"

enum class FlirAlarmMonitoringState
{
    ReadyToCheckAlarm,
    CheckingAlaramState,
    WaitingForMeasFuncType,
    WaitingForMeasFuncId
};

class QnFlirEIPResource:
    public nx::vms::server::resource::Camera
{
    Q_OBJECT
public:

    QnFlirEIPResource(QnMediaServerModule* serverModule);
    ~QnFlirEIPResource();

    struct PortTimerEntry
    {
        QString portId;
        bool state;
    };

    enum class ParamsRequestMode{
        GetMode, SetMode
    };

    static QByteArray PASSTHROUGH_EPATH();
    static const QString MANUFACTURE;

    boost::optional<QString> getApiParameter(const QString& id);
    bool setApiParameter(const QString& id, const QString& value);

    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDriver(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual QString getDriverName() const override;
    virtual bool  hasDualStreamingInternal() const override;

    virtual bool setOutputPortState(
        const QString& outputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;

protected:
    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;

private:
    nx::vms::server::resource::ApiSingleAdvancedParametersProvider<QnFlirEIPResource> m_advancedParametersProvider;
    mutable QnMutex m_ioMutex;
    mutable QnMutex m_alarmMutex;

    QnIOPortDataList m_inputPorts;
    QnIOPortDataList m_outputPorts;

    QSet<QString> m_supportedMeasFuncs;

    std::deque<bool> m_inputPortStates;
    std::deque<bool> m_alarmStates;
    bool m_inputPortMonitored;

    quint64 m_checkInputPortStatusTimerId;
    quint64 m_checkAlarmStatusTimerId;

    size_t m_currentCheckingPortNumber;
    size_t m_currentCheckingAlarmNumber;
    QString m_currentCheckingMeasFuncType;

    FlirAlarmMonitoringState m_currentAlarmMonitoringState;

    std::shared_ptr<EIPAsyncClient> m_eipAsyncClient;
    std::shared_ptr<EIPAsyncClient> m_outputEipAsyncClient;
    std::shared_ptr<EIPAsyncClient> m_alarmsEipAsyncClient;
    std::map<quint64, PortTimerEntry> m_autoResetTimers;

private:
    MessageRouterRequest buildEIPGetRequest(const QnCameraAdvancedParameter& param) const;
    MessageRouterRequest buildEIPSetRequest(const QnCameraAdvancedParameter& param, const QString& value) const;
    MessageRouterRequest buildEIPOutputPortRequest(const QString& portId, bool portState) const;
    MessageRouterRequest buildPassthroughGetRequest(quint8 serviceCode, const QString& variableName) const;

    QByteArray encodePassthroughVariableName(const QString& variableName) const;
    QByteArray encodeGetParamData(const QnCameraAdvancedParameter& param) const;
    QByteArray encodeSetParamData(const QnCameraAdvancedParameter& param, const QString& data) const;

    bool commitParam(const QnCameraAdvancedParameter& param, SimpleEIPClient* eipClient);

    QString parseAsciiEIPResponse(const MessageRouterResponse& response) const;
    quint32 parseInt32EIPResponse(const MessageRouterResponse& response) const;
    QString parseEIPResponse(const MessageRouterResponse& response, const QnCameraAdvancedParameter& param) const;


    QString getParamDataType(const QnCameraAdvancedParameter& param) const;
    quint8 getServiceCodeByType(const QString& type, const ParamsRequestMode& mode) const;
    bool isPassthroughParam(const QnCameraAdvancedParameter& param) const;
    CIPPath parseParamCIPPath(const QnCameraAdvancedParameter& param) const;

    bool handleButtonParam(const QnCameraAdvancedParameter& param, SimpleEIPClient* eipClient);

    void fetchAndSetAdvancedParameters();
    QString getAdvancedParametersTemplate() const;
    bool loadAdvancedParametersTemplateFromFile(QnCameraAdvancedParams& params, const QString& filename);
    QSet<QString> calculateSupportedAdvancedParameters(const QnCameraAdvancedParams& allParams) const;

    quint8 getInputPortCIPAttribute(size_t portNum) const;
    quint8  getOutputPortCIPAttributeById(const QString& portId) const;

    bool isMeasFunc(const QnIOPortData& port) const;
    QString getMeasFuncType(const QnIOPortData& port) const;
    bool findAlarmInputByTypeAndId(int id, const QString& type, QnIOPortData& portFound) const;

    void initializeIO();
    bool startAlarmMonitoringAsync();
    void scheduleNextAlarmCheck();

    void checkAlarmStatus();
    void checkAlarmStatusDone();
    void getAlarmMeasurementFuncType();
    void getAlarmMeasurementFuncTypeDone();
    void getAlarmMeasurementFuncId();
    void getAlarmMeasurementFuncIdDone();
    void checkInputPortStatus();

    virtual std::vector<Camera::AdvancedParametersProvider*> advancedParametersProviders() override;

private slots:
    void checkInputPortStatusDone();
    void routeAlarmMonitoringFlow();
};

#endif // ENABLE_FLIR
