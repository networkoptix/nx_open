#include "io_controller.h"

#include <utils/common/synctime.h>

namespace nx::vms::server::nvr::hanwha {

static const QString kInputIdPrefix("DI");
static const QString kOutputIdPrefix("DO");

static constexpr int kInputCount = 4;
static constexpr int kOutputCount = 2;

QnIOPortDataList IoController::portDesriptiors() const
{
    QnIOPortDataList result;
    for (int i = 0; i < kInputCount; ++i)
    {
        QnIOPortData inputPort;
        inputPort.id = kInputIdPrefix + QString::number(i);
        inputPort.inputName = lm("Alarm Input %1").args(i + 1);
        inputPort.portType = Qn::IOPortType::PT_Input;
        inputPort.supportedPortTypes = Qn::IOPortType::PT_Input;
        inputPort.iDefaultState = Qn::IODefaultState::IO_OpenCircuit;

        result.push_back(std::move(inputPort));
    }

    for (int i = 0; i < kOutputCount; ++i)
    {
        QnIOPortData outputPort;
        outputPort.id = kOutputIdPrefix + QString::number(i);
        outputPort.outputName = lm("Alarm Output %1").args(i + 1);
        outputPort.portType = Qn::IOPortType::PT_Output;
        outputPort.supportedPortTypes = Qn::IOPortType::PT_Output;
        outputPort.iDefaultState = Qn::IODefaultState::IO_OpenCircuit;

        result.push_back(std::move(outputPort));
    }

    return result;
}

QnIOStateDataList IoController::setOutputPortStates(
    const QnIOStateDataList& portStates,
    std::chrono::milliseconds autoResetTimeout)
{
    // TODO: #dmishin implement.
    return portStates;
}

QnIOStateDataList IoController::portStates() const
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

QnUuid IoController::registerStateChangeHandler(StateChangeHandler handler)
{
    const QnUuid id = QnUuid::createUuid();

    QnMutexLocker lock(&m_handlerMutex);
    m_handlers.emplace(id, std::move(handler));

    return id;
}

void IoController::unregisterStateChangeHandler(QnUuid handlerId)
{
    QnMutexLocker lock(&m_handlerMutex);
    m_handlers.erase(handlerId);
}

} // namespace nx::vms::server::nvr::hanwha
