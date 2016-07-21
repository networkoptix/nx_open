#pragma once

#ifdef ENABLE_ADVANTECH

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

    virtual QnIOStateDataList getPortStates() const override;

    virtual void setInputPortStateChangeCallback(InputStateChangeCallback callback) override;

    virtual void terminate() override;

private:
    bool initializeIO();

    void fetchAllPortStates();

    void processAllPortStatesResponse(const nx_modbus::ModbusResponse& response);
    void updatePortState(size_t bitIndex, const QByteArray& bytes, size_t portIndex);

    bool getBitValue(const QByteArray& bytes, quint64 bitIndex) const;
    quint32 getPortCoil(const QString& ioPortId, bool& success) const;

    void routeMonitoringFlow(nx_modbus::ModbusResponse response);
    void handleMonitoringError();
    void scheduleMonitoringIteration();

private:
    QnResource* m_resource;

    std::atomic<bool> m_monitoringIsInProgress;
    mutable QnMutex m_mutex;

    bool m_ioPortInfoFetched;

    quint64 m_inputMonitorTimerId;

    QnIOPortDataList m_inputs;
    QnIOPortDataList m_outputs;

    QnIOStateDataList m_ioStates;

    nx_modbus::QnModbusAsyncClient m_client;
    nx_modbus::QnModbusClient m_outputClient;
    InputStateChangeCallback m_inputStateChangedCallback;
};

#endif //< ENABLE_ADVANTECH