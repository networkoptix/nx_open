#pragma once

#include <nx/utils/uuid.h>

#include <nx/vms/server/nvr/types.h>
#include <nx/vms/server/nvr/hanwha/common.h>

namespace nx::vms::server::nvr::hanwha {

template<typename PortStateList>
double calculateCurrentConsumptionWatts(const PortStateList& portStates)
{
    double totalConsumptionWatts = 0.0;
    for (const auto& portState: portStates)
        totalConsumptionWatts += portState.devicePowerConsumptionWatts;

    return totalConsumptionWatts;
}

template<typename HandlerMap>
QnUuid getId(const HandlerMap& handlerMap)
{
    QnUuid handlerId;
    do {
        handlerId = QnUuid::createUuid();
    } while (handlerMap.find(handlerId) != handlerMap.cend());

    return handlerId;
}

inline QString makeInputId(int index) { return kInputIdPrefix + QString::number(index); }
inline QString makeOutputId(int index) { return kOutputIdPrefix + QString::number(index); }

} // namespace nx::vms::server::nvr::hanwha
