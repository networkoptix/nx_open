#pragma once

#include <core/resource/resource_fwd.h>
#include <api/model/api_ioport_data.h>


namespace nx_io_managment
{

enum class IOPortState
{
    active,
    activeLow,
    activeHigh,
    nonActive,
};

bool isActiveIOPortState(IOPortState state);

IOPortState fromBoolToIOPortState(bool state);

IOPortState fromDefaultPortState(Qn::IODefaultState state);

}

class QnAbstractIOManager
{
    
public:

    typedef std::function<void(
        QString portId, 
        nx_io_managment::IOPortState portState)> InputStateChangeCallback;

    typedef std::function<void(
        QString reason, bool isFatal)> NetworkIssueCallback;

    virtual ~QnAbstractIOManager() {}

    virtual bool startIOMonitoring() = 0;

    virtual void stopIOMonitoring() = 0;

    virtual bool setOutputPortState(const QString& portId, bool isActive) = 0;

    virtual bool isMonitoringInProgress() const = 0;

    virtual QnIOPortDataList getOutputPortList() const = 0;

    virtual QnIOPortDataList getInputPortList() const = 0;

    virtual QnIOStateDataList getPortStates() const = 0;

    virtual nx_io_managment::IOPortState getPortDefaultState(const QString& portId) const = 0;

    virtual void setPortDefaultState(const QString& portId, nx_io_managment::IOPortState state) = 0;

    virtual void setInputPortStateChangeCallback(InputStateChangeCallback callback) = 0;

    virtual void setNetworkIssueCallback(NetworkIssueCallback callback) = 0;

    virtual void terminate() = 0;
};