#pragma once

#include <deque>

#include <core/resource/resource_fwd.h>
#include <modbus/modbus_async_client.h>
#include <modbus/modbus_client.h>
#include <plugins/common_interfaces/abstract_io_manager.h>

class QnAdamModbusIOManager: public QObject, public QnAbstractIOManager
{
    Q_OBJECT
public: 

    // Resource is not owned by IO manager. Maybe weak pointer would be better.
    explicit QnAdamModbusIOManager(QnResource* ptr);

    virtual ~QnAdamModbusIOManager();

    virtual bool startIOMonitoring() override;

    virtual void stopIOMonitoring() override;

    virtual bool setOutputPortState(const QString& outputId, bool isActive) override;

    virtual bool isMonitoringInProgress() const override;

    virtual QnIOPortDataList getInputPortList() const override;

    virtual QnIOPortDataList getOutputPortList() const override;

    virtual void setInputPortStateChangeCallback(InputStateChangeCallback callback) override;

private:
    quint32 getPortCoil(const QString& ioPortId, bool& success) const;
    bool initializeIO();
    void fetchCurrentInputStates();
    void processInputStatesResponse(const nx_modbus::ModbusResponse& response);

    void scheduleMonitoringIteration();

    void routeMonitoringFlow(nx_modbus::ModbusResponse response);
    void handleMonitoringError();

private:
    QnResource* m_resource;

    std::atomic<bool> m_monitoringIsInProgress;
    bool m_ioPortInfoFetched;

    // All coils holding information about inputs are located in continuous address range
    quint16 m_firstInputCoilAddress;
    quint64 m_inputMonitorTimerId;

    QnIOPortDataList m_inputs;
    QnIOPortDataList m_outputs;

    std::deque<bool> m_inputStates;

    nx_modbus::QnModbusAsyncClient m_client;
    nx_modbus::QnModbusClient m_outputClient;
    InputStateChangeCallback m_inputStateChangedCallback;
};