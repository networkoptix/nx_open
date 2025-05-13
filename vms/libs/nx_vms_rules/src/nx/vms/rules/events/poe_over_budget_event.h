// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API PoeOverBudgetEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "poeOverBudget")
    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(double, currentConsumptionW, setCurrentConsumptionW)
    FIELD(double, upperLimitW, setUpperLimitW)
    FIELD(double, lowerLimitW, setLowerLimitW)

public:
    PoeOverBudgetEvent() = default;
    PoeOverBudgetEvent(
        std::chrono::microseconds timestamp,
        State state,
        nx::Uuid serverId,
        double currentConsumptionW,
        double upperLimitW,
        double lowerLimitW);

    virtual QString aggregationKey() const override { return m_serverId.toSimpleString(); }
    virtual QVariantMap details(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

    static QString overallConsumption(double current, double limit);

    static const ItemDescriptor& manifest();

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

private:
    QString description() const;
    QStringList detailing() const;

    bool isEmpty() const;
    QString overallConsumption() const;
};

} // namespace nx::vms::rules
