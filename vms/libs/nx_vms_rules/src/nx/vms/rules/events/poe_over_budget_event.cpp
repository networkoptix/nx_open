// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_over_budget_event.h"

#include <nx/vms/common/html/html.h>

#include "../event_filter_fields/source_server_field.h"
#include "../group.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

namespace {

QString formatValue(double value)
{
    return QString::number(value, 'f', 1);
}

} // namespace

PoeOverBudgetEvent::PoeOverBudgetEvent(
    std::chrono::microseconds timestamp,
    State state,
    nx::Uuid serverId,
    double currentConsumptionW,
    double upperLimitW,
    double lowerLimitW)
    :
    BasicEvent(timestamp, state),
    m_serverId(serverId),
    m_currentConsumptionW(currentConsumptionW),
    m_upperLimitW(upperLimitW),
    m_lowerLimitW(lowerLimitW)
{
}

QString PoeOverBudgetEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QVariantMap PoeOverBudgetEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kDescriptionDetailName, description());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::poeSettings);

    return result;
}

QString PoeOverBudgetEvent::overallConsumption() const
{
    if (isEmpty())
        return {};

    return nx::format("%1 W / %2 W",
        formatValue(m_currentConsumptionW),
        formatValue(m_upperLimitW));
}

QString PoeOverBudgetEvent::description() const
{
    const auto consumption = overallConsumption();
    if (consumption.isEmpty())
        return {};

    return QString("%1 %2").arg(common::html::bold(tr("Consumption"))).arg(consumption);
}

QString PoeOverBudgetEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(m_serverId, Qn::RI_WithUrl);
    return tr("PoE over budget at %1").arg(resourceName);
}

QString PoeOverBudgetEvent::detailing() const
{
    const auto consumption = overallConsumption();
    if (consumption.isEmpty())
        return {};

    return tr("Reason: Power limit exceeded (%1)", "%1 is consumption").arg(consumption);
}

const ItemDescriptor& PoeOverBudgetEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PoeOverBudgetEvent>(),

        .groupId = kServerIssueEventGroup,
        .displayName = tr("PoE Over Budget"),
        .flags = ItemFlag::prolonged,
        .fields = {
            utils::makeStateFieldDescriptor(tr("Begin When")),
            makeFieldDescriptor<SourceServerField>(utils::kServerIdFieldName, tr("Server")),
        },
        .resources = {{utils::kServerIdFieldName, {ResourceType::server}}},
        .readPermissions = GlobalPermission::powerUser,
        .emailTemplatePath = ":/email_templates/poe_over_budget.mustache",
        .serverFlags = {api::ServerFlag::SF_HasPoeManagementCapability}
    };
    return kDescriptor;
}

bool PoeOverBudgetEvent::isEmpty() const
{
    return qFuzzyIsNull(m_currentConsumptionW)
        && qFuzzyIsNull(m_upperLimitW)
        && qFuzzyIsNull(m_lowerLimitW);
}

} // namespace nx::vms::rules
