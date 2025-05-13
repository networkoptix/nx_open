// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_over_budget_event.h"

#include <nx/vms/common/html/html.h>

#include "../event_filter_fields/source_server_field.h"
#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
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

QVariantMap PoeOverBudgetEvent::details(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, detailLevel);
    fillAggregationDetailsForServer(result, context, serverId(), detailLevel);

    utils::insertIfNotEmpty(result, utils::kDescriptionDetailName, description());
    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::poeSettings);
    utils::insertIcon(result, nx::vms::rules::Icon::networkIssue);

    result[utils::kCaptionDetailName] = manifest().displayName();

    const QStringList details(detailing());
    result[utils::kDescriptionDetailName] = details.join('\n');
    result[utils::kDetailingDetailName] = details;
    result[utils::kHtmlDetailsName] = details;

    return result;
}

QString PoeOverBudgetEvent::overallConsumption(double current, double limit)
{
    return nx::format("%1 W / %2 W",
        formatValue(current),
        formatValue(limit));
}

QString PoeOverBudgetEvent::overallConsumption() const
{
    if (isEmpty())
        return {};

    return overallConsumption(m_currentConsumptionW, m_upperLimitW);
}

QString PoeOverBudgetEvent::description() const
{
    const auto consumption = overallConsumption();
    if (consumption.isEmpty())
        return {};

    return QString("%1 %2").arg(common::html::bold(tr("Consumption"))).arg(consumption);
}

QString PoeOverBudgetEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, serverId(), detailLevel);
    return tr("PoE over budget on %1").arg(resourceName);
}

QStringList PoeOverBudgetEvent::detailing() const
{
    return QStringList{
        tr("Current power consumption: %1 watts").arg(m_currentConsumptionW, 0, 'f', 1),
        tr("Upper consumption limit: %1 watts").arg(m_upperLimitW, 0, 'f', 1),
        tr("Lower consumption limit: %1 watts").arg(m_lowerLimitW, 0, 'f', 1)
    };
}

const ItemDescriptor& PoeOverBudgetEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PoeOverBudgetEvent>(),

        .groupId = kServerIssueEventGroup,
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("PoE Over Budget")),
        .description = "Triggered when the Power over Ethernet (PoE) budget is exceeded on the device.",
        .flags = ItemFlag::prolonged,
        .fields = {
            utils::makeStateFieldDescriptor(Strings::beginWhen()),
            makeFieldDescriptor<SourceServerField>(
                utils::kServerIdFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Server")),
                {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kHasPoeManagementValidationPolicy
                }.toVariantMap()),
        },
        .resources = {{utils::kServerIdFieldName, {ResourceType::server}}},
        .readPermissions = GlobalPermission::powerUser,
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
