// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_over_budget_event.h"

#include <nx/vms/common/html/html.h>

#include "../event_fields/source_server_field.h"
#include "../utils/event_details.h"
#include "../utils/type.h"

namespace nx::vms::rules {

QMap<QString, QString> PoeOverBudgetEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kDescriptionDetailName, description());

    return result;
}

QString PoeOverBudgetEvent::description() const
{
    const QString consumptionString; // TODO: #mmalofeev Add consumption params to the PoeOverBudgetEvent.
    if (consumptionString.isEmpty())
        return {};

    return QString("%1 %2").arg(common::html::bold(tr("Consumption"))).arg(consumptionString);
}

const ItemDescriptor& PoeOverBudgetEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PoeOverBudgetEvent>(),
        .displayName = tr("PoE over Budget"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<SourceServerField>("serverId", tr("Server")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
