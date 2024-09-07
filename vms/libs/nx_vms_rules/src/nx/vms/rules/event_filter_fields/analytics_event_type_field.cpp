// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_event_type_field.h"

#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/utils.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/utils/openapi_doc.h>

namespace nx::vms::rules {

AnalyticsEventTypeField::AnalyticsEventTypeField(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    EventFilterField{descriptor},
    SystemContextAware(context)
{
}

bool AnalyticsEventTypeField::match(const QVariant& value) const
{
    const auto eventTypeId = value.toString();
    const auto taxonomyState = systemContext()->analyticsTaxonomyState();

    if (eventTypeId.isEmpty() || !taxonomyState)
        return false;

    const auto eventType = taxonomyState->eventTypeById(eventTypeId);
    if (!eventType)
        return false;

    return (eventTypeId == m_typeId)
        || nx::analytics::taxonomy::eventBelongsToGroup(eventType, m_typeId);
}

nx::Uuid AnalyticsEventTypeField::engineId() const
{
    return m_engineId;
}

void AnalyticsEventTypeField::setEngineId(nx::Uuid id)
{
    if (m_engineId != id)
    {
        m_engineId = id;
        emit engineIdChanged();
    }
}

QString AnalyticsEventTypeField::typeId() const
{
    return m_typeId;
}

void AnalyticsEventTypeField::setTypeId(const QString& id)
{
    if (m_typeId != id)
    {
        m_typeId = id;
        emit typeIdChanged();
    }
}

QJsonObject AnalyticsEventTypeField::openApiDescriptor()
{
    auto result =
        utils::constructOpenApiDescriptor<AnalyticsEventTypeField>();
    auto properties = result["properties"].toObject();
    utils::updatePropertyForField(
        properties, "typeId", "example", "nx.stub.lineCrossing");
    result["properties"] = properties;
    return result;
}

} // namespace nx::vms::rules
