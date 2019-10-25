#include "io_module_resource.h"

#include <nx/vms/server/nvr/hanwha/common.h>

#include <utils/common/synctime.h>

namespace nx::vms::server::nvr::hanwha {

static const QString kInputIdPrefix("DI");
static const QString kOutputIdPrefix("DO");

static constexpr int kInputCount = 4;
static constexpr int kOutputCount = 2;

static QnIOPortDataList portDescriptions()
{
    QnIOPortDataList result;
    for (int i = 0; i < kInputCount; ++i)
    {
        QnIOPortData inputPort;
        inputPort.id = kInputIdPrefix + QString::number(i);
        inputPort.inputName = lm("Alarm Input %1").args(i + 1);
        inputPort.portType = Qn::IOPortType::PT_Input;
        inputPort.supportedPortTypes = Qn::IOPortType::PT_Input;
        inputPort.iDefaultState = Qn::IODefaultState::IO_GroundedCircuit;

        result.push_back(std::move(inputPort));
    }

    for (int i = 0; i < kOutputCount; ++i)
    {
        QnIOPortData outputPort;
        outputPort.id = kOutputIdPrefix + QString::number(i);
        outputPort.outputName = lm("Alarm Output %1").args(i + 1);
        outputPort.portType = Qn::IOPortType::PT_Output;
        outputPort.supportedPortTypes = Qn::IOPortType::PT_Output;
        outputPort.iDefaultState = Qn::IODefaultState::IO_GroundedCircuit;

        result.push_back(std::move(outputPort));
    }

    return result;
}

IoModuleResource::IoModuleResource(QnMediaServerModule* serverModule):
    nx::vms::server::resource::Camera(serverModule)
{
}

CameraDiagnostics::Result IoModuleResource::initializeCameraDriver()
{
    setIoPortDescriptions(portDescriptions(), /*needMerge*/ false);
    setProperty(ResourcePropertyKey::kIoConfigCapability, QString("1"));
    setFlags(flags() | Qn::io_module);

    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

QString IoModuleResource::getDriverName() const
{
    return kHanwhaPoeNvrDriverName;
}

QnIOStateDataList IoModuleResource::ioPortStates() const
{
    // TODO: #dmsihin this is fake implementation! Don't forget to replace with the proper one.
    QnIOStateDataList result;
    for (int i = 0; i < kInputCount; ++i)
    {
        QnIOStateData inputState;
        inputState.id = kInputIdPrefix + QString::number(i);
        inputState.isActive = false;
        inputState.timestamp = qnSyncTime->currentMSecsSinceEpoch();

        result.push_back(std::move(inputState));
    }

    for (int i = 0; i < kOutputCount; ++i)
    {
        QnIOStateData outputState;
        outputState.id = kOutputIdPrefix + QString::number(i);
        outputState.isActive = false;
        outputState.timestamp = qnSyncTime->currentMSecsSinceEpoch();

        result.push_back(std::move(outputState));
    }

    return result;
}

bool IoModuleResource::setOutputPortState(
    const QString& outputId, bool isActive, unsigned int autoResetTimeoutMs)
{
    // TODO: #dmishin implement.
    return false;
}

void IoModuleResource::startInputPortStatesMonitoring()
{
    // TODO: #dmishin implement.
}

void IoModuleResource::stopInputPortStatesMonitoring()
{
    // TODO: #dmishin implement.
}

} // namespace nx::vms::server::nvr::hanwha
