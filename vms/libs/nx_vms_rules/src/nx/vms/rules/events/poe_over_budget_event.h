// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API PoeOverBudgetEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.poeOverBudget")

    FIELD(QnUuid, serverId, setServerId)
    FIELD(double, currentConsumptionW, setCurrentConsumptionW)
    FIELD(double, upperLimitW, setUpperLimitW)
    FIELD(double, lowerLimitW, setLowerLimitW)

public:
    PoeOverBudgetEvent() = default;

    PoeOverBudgetEvent(
        std::chrono::microseconds timestamp,
        State state,
        QnUuid serverId,
        double currentConsumptionW,
        double upperLimitW,
        double lowerLimitW);

    virtual QString uniqueName() const override;
    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QString description() const;
    QString extendedCaption(common::SystemContext* context) const;
    QString detailing() const;

    bool isEmpty() const;
    QString overallConsumption() const;
};

} // namespace nx::vms::rules
