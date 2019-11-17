#include "io_platform_abstraction.h"

#include <map>

#include <utils/common/synctime.h>

#include <nx/utils/log/log.h>

#include <nx/vms/server/nvr/hanwha/utils.h>
#include <nx/vms/server/nvr/hanwha/common.h>
#include <nx/vms/server/nvr/hanwha/ioctl_common.h>

namespace nx::vms::server::nvr::hanwha {

#if defined (Q_OS_LINUX)

static const std::map<QString, int> kGetInputStateCommandByPortId = {
    {makeInputId(0), kGetAlarmInput0Command},
    {makeInputId(1), kGetAlarmInput1Command},
    {makeInputId(2), kGetAlarmInput2Command},
    {makeInputId(3), kGetAlarmInput3Command},
};

static const std::map<QString, int> kSetOutputStateCommandByPortId = {
    {makeOutputId(0), kSetAlarmOutput0Command},
    {makeOutputId(1), kSetAlarmOutput1Command}
};

static std::optional<int> getCommandIdByInputPortId(const QString& portId)
{
    if (const auto it = kGetInputStateCommandByPortId.find(portId);
        it != kGetInputStateCommandByPortId.cend())
    {
        return it->second;
    }

    return std::nullopt;
}

static std::optional<int> getCommandIdByOutputPortId(const QString& portId)
{
    if (const auto it = kSetOutputStateCommandByPortId.find(portId);
        it != kSetOutputStateCommandByPortId.cend())
    {
        return it->second;
    }

    return std::nullopt;
}

class IoPlatformAbstractionImpl: public IIoPlatformAbstraction
{
public:
    IoPlatformAbstractionImpl(int ioDeviceDescriptor):
        m_ioDeviceDescriptor(ioDeviceDescriptor)
    {
    }

    ~IoPlatformAbstractionImpl()
    {
    }

    virtual QnIOPortDataList portDescriptors() const override
    {
        QnIOPortDataList result;
        for (int i = 0; i < kInputCount; ++i)
        {
            QnIOPortData inputPort;
            inputPort.id = makeInputId(i);
            inputPort.inputName = lm("Alarm Input %1").args(i + 1);
            inputPort.portType = Qn::IOPortType::PT_Input;
            inputPort.supportedPortTypes = Qn::IOPortType::PT_Input;
            inputPort.iDefaultState = Qn::IODefaultState::IO_OpenCircuit;

            result.push_back(std::move(inputPort));
        }

        for (int i = 0; i < kOutputCount; ++i)
        {
            QnIOPortData outputPort;
            outputPort.id = makeOutputId(i);
            outputPort.outputName = lm("Alarm Output %1").args(i + 1);
            outputPort.portType = Qn::IOPortType::PT_Output;
            outputPort.supportedPortTypes = Qn::IOPortType::PT_Output;
            outputPort.iDefaultState = Qn::IODefaultState::IO_OpenCircuit;

            result.push_back(std::move(outputPort));
        }

        return result;
    }

    virtual bool setOutputPortState(const QString& portId, IoPortState state) override
    {
        const std::optional<int> commandId = getCommandIdByOutputPortId(portId);
        if (!commandId)
        {
            NX_WARNING(this, "Unable to find a command to change port '%1' state", portId);
            return false;
        }

        const int command = prepareCommand<int>(*commandId);

        int commandData = (state == IoPortState::active);
        const int result = ::ioctl(m_ioDeviceDescriptor, command, &commandData);
        if (result != 0)
        {
            NX_WARNING(this, "Unable to %1 port '%2'",
                ((state == IoPortState::active) ? "enable" : "disable"),
                portId);

            return false;
        }

        return true;
    }

    virtual QnIOStateData portState(const QString& portId) const override
    {
        const std::optional<int> commandId = getCommandIdByInputPortId(portId);
        const int timestamp = qnSyncTime->currentMSecsSinceEpoch();
        if (!commandId)
        {
            NX_WARNING(this, "Unable to find a command to get port '%1' state", portId);
            return QnIOStateData(portId, /*portState*/ false, timestamp);
        }

        const int command = prepareCommand<int>(*commandId);

        int commandData = 0;
        const int result = ::ioctl(m_ioDeviceDescriptor, command, &commandData);
        if (result != 0)
        {
            NX_WARNING(this, "Unable to get state of the port '%2'", portId);
            return QnIOStateData(portId, /*portState*/ false, timestamp);
        }

        return {portId, commandData, timestamp};
    }
private:
    int m_ioDeviceDescriptor = -1;

};

#else
class IoPlatformAbstractionImpl: public IIoPlatformAbstraction
{
public:
    IoPlatformAbstractionImpl(int /*ioDeviceDescriptor*/)
    {
        NX_ASSERT(false);
    }

    ~IoPlatformAbstractionImpl()
    {
        NX_ASSERT(false);
    }

    virtual QnIOPortDataList portDescriptors() const override
    {
        NX_ASSERT(false);
        return {};
    }

    virtual bool setOutputPortState(const QString& /*portId*/, IoPortState /*state*/) override
    {
        NX_ASSERT(false);
        return false;
    }

    virtual QnIOStateData portState(const QString& /*portId*/) const override
    {
        NX_ASSERT(false);
        return QnIOStateData();
    }
};

#endif

IoPlatformAbstraction::IoPlatformAbstraction(int ioDeviceDescriptor):
    m_impl(std::make_unique<IoPlatformAbstractionImpl>(ioDeviceDescriptor))
{
}

IoPlatformAbstraction::~IoPlatformAbstraction()
{
}

QnIOPortDataList IoPlatformAbstraction::portDescriptors() const
{
    return m_impl->portDescriptors();
}

bool IoPlatformAbstraction::setOutputPortState(const QString& portId, IoPortState state)
{
    return m_impl->setOutputPortState(portId, state);
}

QnIOStateData IoPlatformAbstraction::portState(const QString& portId) const
{
    return m_impl->portState(portId);
}

} // namespace nx::vms::server::nvr::hanwha
