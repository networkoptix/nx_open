#ifndef FLIR_EIP_RESOURCE_H
#define FLIR_EIP_RESOURCE_H

#include "core/resource/security_cam_resource.h"
#include "../onvif/dataprovider/rtp_stream_provider.h"
#include "core/resource/camera_resource.h"
#include <utils/xml/camera_advanced_param_reader.h>
#include <utils/common/timermanager.h>
#include "simple_eip_client.h"
#include "eip_async_client.h"
#include "flir_eip_data.h"

class QnFlirEIPResource : public QnPhysicalCameraResource
{
    Q_OBJECT
public:

    QnFlirEIPResource();
    ~QnFlirEIPResource();

    enum class ParamsRequestMode{
        GetMode, SetMode
    };

    static QByteArray PASSTHROUGH_EPATH();
    static const int IO_CHECK_TIMEOUT = 3000;
    static const QString MANUFACTURE;

    virtual bool getParamPhysical(const QString &id, QString &value) override;
    virtual bool setParamPhysical(const QString &id, const QString& value) override;

    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int, int) override;
    virtual bool  hasDualStreaming() const override;

    virtual QnIOPortDataList getRelayOutputList() const override;
    virtual QnIOPortDataList getInputPortList() const override;

    virtual bool setRelayOutputState(
        const QString& outputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;

protected:
    virtual bool startInputPortMonitoringAsync( std::function<void(bool)>&& completionHandler ) override;
    virtual void stopInputPortMonitoringAsync() override;
    virtual bool isInputPortMonitored() const override;

private:
    QnCameraAdvancedParams m_advancedParameters;
    mutable QnMutex m_physicalParamsMutex;
    mutable QnMutex m_ioMutex;


    QnIOPortDataList m_inputPorts;
    QnIOPortDataList m_outputPorts;

    std::deque<bool> m_inputPortStates;
    bool m_inputPortMonitored;

    quint64 m_checkInputPortStatusTimerId;
    size_t m_currentCheckingPortNumber;
    size_t m_currentRelayIndex;

    std::shared_ptr<SimpleEIPClient> m_eipClient;
    std::shared_ptr<EIPAsyncClient> m_eipAsyncClient;
    std::shared_ptr<EIPAsyncClient> m_outputEipAsyncClient;

private:
    MessageRouterRequest buildEIPGetRequest(const QnCameraAdvancedParameter& param) const;
    MessageRouterRequest buildEIPSetRequest(const QnCameraAdvancedParameter& param, const QString& value) const;
    MessageRouterRequest buildEIPOutputPortRequest(const QString& portId, bool portState) const;

    QByteArray encodeGetParamData(const QnCameraAdvancedParameter& param) const;
    QByteArray encodeSetParamData(const QnCameraAdvancedParameter& param, const QString& data) const;

    bool commitParam(const QnCameraAdvancedParameter& param);

    QString parseEIPResponse(const MessageRouterResponse& response, const QnCameraAdvancedParameter& param) const;

    QString getParamDataType(const QnCameraAdvancedParameter& param) const;
    quint8 getServiceCodeByType(const QString& type, const ParamsRequestMode& mode) const;
    bool isPassthroughParam(const QnCameraAdvancedParameter& param) const;
    CIPPath parseParamCIPPath(const QnCameraAdvancedParameter& param) const;

    bool handleButtonParam(const QnCameraAdvancedParameter& param);

    void fetchAndSetAdvancedParameters();
    QString getAdvancedParametersTemplate() const;
    bool loadAdvancedParametersTemplateFromFile(QnCameraAdvancedParams& params, const QString& filename);
    QSet<QString> calculateSupportedAdvancedParameters(const QnCameraAdvancedParams& allParams) const;

    quint8 getInputPortCIPAttribute(size_t portNum) const;
    quint8  getOutputPortCIPAttributeById(const QString& portId) const;

    void initializeIO();
    QnIOPortDataList mergeIOPorts() const;

private slots:
    void checkInputPortStatusDone();
    void checkInputPortStatus(quint64 timerId);


};

#endif // FLIR_EIP_RESOURCE_H
