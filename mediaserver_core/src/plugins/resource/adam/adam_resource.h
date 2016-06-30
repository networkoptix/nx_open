#pragma once

#include <QtCore/QMap>
#include <utils/thread/mutex.h>
#include <atomic>

#include <core/resource/security_cam_resource.h>
#include <utils/common/timermanager.h>
#include <modbus/modbus_async_client.h>


class QnAdamResource : public QnSecurityCamResource
{
    Q_OBJECT
public:
    static const QString kManufacture;

    QnAdamResource();
    ~QnAdamResource();

    virtual void setIframeDistance(
        int frames,
        int timeMs) override
    {
        QN_UNUSED(frames);
        QN_UNUSED(timeMs);
    }

    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QnIOPortDataList getRelayOutputList() const override;

    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QnIOPortDataList getInputPortList() const override;

    //!Implementation of QnSecurityCamResource::setRelayOutputState
    /*!
        Actual request is performed asynchronously. This method only posts task to the queue
    */
    virtual bool setRelayOutputState(
        const QString& outputID,
        bool isActive,
        unsigned int autoResetTimeoutMS ) override;

    //!Implementation of TimerEventHandler::onTimer
    virtual void onTimer( const quint64& timerID );

protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override {}

    //!Implementation of QnSecurityCamResource::startInputPortMonitoringAsync
    virtual bool startInputPortMonitoringAsync(
            std::function<void(bool)>&& completionHandler ) override;

    //!Implementation of QnSecurityCamResource::stopInputPortMonitoringAsync
    virtual void stopInputPortMonitoringAsync() override;

    //!Implementation of QnSecurityCamResource::isInputPortMonitored
    virtual bool isInputPortMonitored() const override;

private:
    void fetchIOPorts();

    bool requestInputValues();

    void routeMonitoringFlow();

    void handleMonitoringError();

private:
    QnIOPortDataList m_inputs;
    QnIOPortDataList m_outputs;
    std::atomic<bool> m_inputsMonitored;
    nx_modbus::QnModbusAsyncClient m_modbusAsyncClient;


};
