#include "io_controller.h"

#include <nx/vms/server/nvr/hanwha/io_state_fetcher.h>
#include <nx/vms/server/nvr/hanwha/common.h>

namespace nx::vms::server::nvr::hanwha {

static QnIOStateDataList calculateChangedPortStates(
    const std::set<QnIOStateData>& oldState,
    const std::set<QnIOStateData>& newState)
{
    QnIOStateDataList result;
    for (const auto& newPortState: newState)
    {
        if (const auto it = oldState.find(newPortState);
            it == oldState.cend() || it->isActive != newPortState.isActive)
        {
            result.push_back(newPortState);
        }
    }

    return result;
}

static QnIOStateDataList toList(const std::set<QnIOStateData>& state)
{
    QnIOStateDataList result;
    for (const auto& portState: state)
        result.push_back(portState);

    return result;
}

IoController::IoController():
    m_stateFetcher(std::make_unique<IoStateFetcher>(
        [this](const std::set<QnIOStateData>& state) { handleState(state); }))
{
}

IoController::~IoController()
{
    m_stateFetcher->stop();
}

void IoController::start()
{
    m_stateFetcher->start();
}

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
    QnMutexLocker lock(&m_mutex);
    return toList(m_lastIoState);
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

void IoController::handleState(const std::set<QnIOStateData>& state)
{
    QnIOStateDataList changedPortStates;
    {
        QnMutexLocker lock(&m_mutex);
        if (state == m_lastIoState)
            return;

        changedPortStates = calculateChangedPortStates(state, m_lastIoState);
        m_lastIoState = state;
    }

    if (changedPortStates.empty())
        return;

    {
        QnMutexLocker lock(&m_handlerMutex);
        for (const auto& [_, handler]: m_handlers)
            handler(changedPortStates);
    }
}

} // namespace nx::vms::server::nvr::hanwha
