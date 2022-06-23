// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/vms/event/events/prolonged_event.h>
#include <nx/vms/event/events/events_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::event {

class NX_VMS_COMMON_API PoeOverBudgetEvent: public ProlongedEvent
{
    using base_type = ProlongedEvent;

public:
    struct Parameters
    {
        double currentConsumptionWatts = 0.0;
        double upperLimitWatts = 0.0;
        double lowerLimitWatts = 0.0;

        bool isEmpty() const;
    };

#define nx_vms_event_PoeOverBudgetEvent_Parameters_Fields \
    (currentConsumptionWatts) \
    (upperLimitWatts) \
    (lowerLimitWatts)

public:
    PoeOverBudgetEvent(
        QnMediaServerResourcePtr sourceServer,
        EventState eventState,
        std::chrono::microseconds timestamp,
        double currentConsumptionWatts,
        double upperLimitWatts,
        double lowerLimitWatts);

    virtual EventParameters getRuntimeParams() const override;

    virtual bool checkEventParams(const EventParameters &params) const override;

    static Parameters consumptionParameters(const EventParameters &params);

    const Parameters& poeParameters() const;

private:
    Parameters m_parameters;
};

QN_FUSION_DECLARE_FUNCTIONS(PoeOverBudgetEvent::Parameters, (json), NX_VMS_COMMON_API)

} // namespace nx::vms::event

Q_DECLARE_METATYPE(nx::vms::event::PoeOverBudgetEventPtr);
