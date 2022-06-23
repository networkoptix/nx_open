// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_over_budget_event.h"

#include <nx/vms/common/html/html.h>

#include "../event_fields/source_server_field.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

QVariantMap PoeOverBudgetEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kDescriptionDetailName, description());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString PoeOverBudgetEvent::description() const
{
    const QString consumptionString; // TODO: #mmalofeev Add consumption params to the PoeOverBudgetEvent.
    if (consumptionString.isEmpty())
        return {};

    return QString("%1 %2").arg(common::html::bold(tr("Consumption"))).arg(consumptionString);
}

QString PoeOverBudgetEvent::extendedCaption(common::SystemContext* context) const
{
    if (totalEventCount() == 1)
    {
        const auto resourceName = utils::StringHelper(context).resource(m_serverId, Qn::RI_WithUrl);
        return tr("PoE over budget at %1").arg(resourceName);
    }

    return BasicEvent::extendedCaption();
}

const ItemDescriptor& PoeOverBudgetEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PoeOverBudgetEvent>(),
        .displayName = tr("PoE over Budget"),
        .flags = ItemFlag::prolonged,
        .fields = {
            utils::makeStateFieldDescriptor(tr("State")),
            makeFieldDescriptor<SourceServerField>(utils::kServerIdFieldName, tr("Server")),
        },
        .emailTemplatePath = ":/email_templates/poe_over_budget.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
