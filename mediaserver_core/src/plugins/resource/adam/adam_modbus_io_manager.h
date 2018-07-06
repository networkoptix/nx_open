#pragma once

#ifdef ENABLE_ADVANTECH

#include <deque>

#include <core/resource/resource_fwd.h>
#include <modbus/modbus_async_client.h>
#include <modbus/modbus_client.h>
#include <plugins/common_interfaces/abstract_io_manager.h>
#include <nx/utils/safe_direct_connection.h>

class QnAdamModbusIOManager:
    public QObject,
    public Qn::EnableSafeDirectConnection,
    public QnAbstractIOManager
{
    struct PortStateChangeInfo
    {
        PortStateChangeInfo(nx_io_managment::IOPortState _state, bool _isChanged):
            state(_state),
            isChanged(_isChanged)
        {}

        nx_io_managment::IOPortState state;
        bool isChanged;
    };

    struct DebouncedValue
    {
        bool debouncedValue;
        int lifetimeCounter;
    };

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

    virtual nx_io_managment::IOPortState getPortDefaultState(const QString& portId) const override;

    virtual void setPortDefaultState(
        const QString& portId, 
        nx_io_managment::IOPortState state) override;

    virtual void setInputPortStateChangeCallback(InputStateChangeCallback callback) override;

    virtual void setNetworkIssueCallback(NetworkIssueCallback callback) override;

    virtual void terminate() override;

private:
    bool initializeIO();

    void fetchAllPortStates();
    void fetchAllPortStatesUnsafe();

    void processAllPortStatesResponse(const nx::modbus::ModbusMessage& response);
    PortStateChangeInfo updatePortState(size_t bitIndex, const QByteArray& bytes, size_t portIndex);
    void setDebounceForPort(const QString& portId, bool portState);
    QnIOStateDataList getDebouncedStates() const;

    bool getBitValue(const QByteArray& bytes, quint64 bitIndex) const;
    quint32 getPortCoil(const QString& ioPortId, bool& success) const;

    void routeMonitoringFlow(nx::modbus::ModbusMessage response);
    void handleMonitoringError();
    void scheduleMonitoringIteration();

    nx_io_managment::IOPortState getPortDefaultStateUnsafe(const QString& portId) const;

private:
    QnResource* m_resource;

    std::atomic<bool> m_monitoringIsInProgress;
    mutable QnMutex m_mutex;

    bool m_ioPortInfoFetched;

    quint64 m_inputMonitorTimerId;

    QnIOPortDataList m_inputs;
    QnIOPortDataList m_outputs;

    QnIOStateDataList m_ioStates;

    nx::modbus::QnModbusAsyncClient m_client;
    nx::modbus::QnModbusClient m_outputClient;
    InputStateChangeCallback m_inputStateChangedCallback;
    NetworkIssueCallback m_networkIssueCallback;

    std::atomic<int> m_networkFaultsCounter;

    QMap<QString, nx_io_managment::IOPortState> m_defaultPortStates;

    mutable std::map<QString, DebouncedValue> m_debouncedValues;
};

#endif //< ENABLE_ADVANTECH
