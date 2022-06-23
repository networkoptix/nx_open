// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_over_budget_event.h"

#include <nx/fusion/model_functions.h>
#include <core/resource/media_server_resource.h>

namespace nx::vms::event {

PoeOverBudgetEvent::PoeOverBudgetEvent(
    QnMediaServerResourcePtr sourceServer,
    EventState eventState,
    std::chrono::microseconds timestamp,
    double currentConsumptionWatts,
    double upperLimitWatts,
    double lowerLimitWatts)
    :
    ProlongedEvent(EventType::poeOverBudgetEvent, sourceServer, eventState, timestamp.count()),
    m_parameters({currentConsumptionWatts, upperLimitWatts, lowerLimitWatts})
{
}

EventParameters PoeOverBudgetEvent::getRuntimeParams() const
{
    EventParameters eventParameters = base_type::getRuntimeParams();
    eventParameters.inputPortId = QJson::serialized<Parameters>(m_parameters);

    return eventParameters;
}

bool PoeOverBudgetEvent::checkEventParams(const EventParameters& /*params*/) const
{
    // TODO: #dmishin implement or remove.
    return true;
}

PoeOverBudgetEvent::Parameters PoeOverBudgetEvent::consumptionParameters(
    const EventParameters &params)
{
    return QJson::deserialized(params.inputPortId.toLatin1(), Parameters());
}

bool PoeOverBudgetEvent::Parameters::isEmpty() const
{
    return qFuzzyIsNull(currentConsumptionWatts)
        && qFuzzyIsNull(upperLimitWatts)
        && qFuzzyIsNull(lowerLimitWatts);
}

const PoeOverBudgetEvent::Parameters& PoeOverBudgetEvent::poeParameters() const
{
    return m_parameters;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PoeOverBudgetEvent::Parameters, (json),
    nx_vms_event_PoeOverBudgetEvent_Parameters_Fields, (brief, true))

} // namespace nx::vms::event
