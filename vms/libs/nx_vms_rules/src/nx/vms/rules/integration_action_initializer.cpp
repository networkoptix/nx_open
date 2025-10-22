// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_action_initializer.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/analytics/integration_action.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/ini.h>

#include "actions/integration_action.h"
#include "utils/field.h"

namespace {
    nx::vms::rules::ItemDescriptor itemDescriptorFromIntegrationAction(
        const QString& actionId,
        const nx::vms::api::analytics::IntegrationAction& integrationAction)
    {
        QList<nx::vms::rules::FieldDescriptor> fieldDescriptors;
        for (const auto& fieldModel: integrationAction.fields)
        {
            nx::vms::rules::FieldDescriptor descriptor{
                .type = fieldModel.type,
                .fieldName = fieldModel.fieldName,
                .displayName = nx::TranslatableString(fieldModel.displayName),
                .description = fieldModel.description,
                .properties = fieldModel.properties.toVariantMap(),
            };
            fieldDescriptors.append(std::move(descriptor));
        }

        return nx::vms::rules::ItemDescriptor{
            .id = actionId,
            .displayName = nx::TranslatableString(integrationAction.name),
            .description = integrationAction.description,
            .flags =
            {
                nx::vms::rules::ItemFlag::dynamic,
                integrationAction.isInstant
                    ? nx::vms::rules::ItemFlag::instant
                    : nx::vms::rules::ItemFlag::prolonged,
            },
            .fields = std::move(fieldDescriptors),
        };
    }
} // namespace

namespace nx::vms::rules {

IntegrationActionInitializer::IntegrationActionInitializer(
    nx::vms::common::SystemContext* context)
    :
    SystemContextAware(context)
{
}

IntegrationActionInitializer::~IntegrationActionInitializer()
{
}

void IntegrationActionInitializer::registerActions() const
{
    if (!ini().integrationActions)
        return;

    for (auto& engine: resourcePool()->getResources<nx::vms::common::AnalyticsEngineResource>())
    {
        for (auto& actionDescriptor: engine->manifest().integrationActions)
        {
            const QString actionType = kIntegrationActionTypePrefix + actionDescriptor.id;
            m_engine->registerAction(
                itemDescriptorFromIntegrationAction(actionType, actionDescriptor),
                [actionType, fields = actionDescriptor.fields]
                {
                    const auto action = new IntegrationAction(actionType);
                    // Since ActionBuilder::buildActionCreate iterates over writable properties of
                    // the object to find fields of the Action, we have to create dynamic
                    // properties for all the fields here. TODO: Change buildAction to use
                    // explicit field descriptors from the manifest instead and remove this code.
                    for (const auto& field: fields)
                    {
                        const auto fieldName = field.fieldName.toStdString();
                        action->setProperty(fieldName.c_str(), "");
                    }
                    return action;
                });

            m_registeredActionTypes.insert(actionType);
        }
    }
}

void IntegrationActionInitializer::registerFields() const
{
    registerActionField<StringSelectionField>();
}

void IntegrationActionInitializer::deinitialize()
{
    if (!m_engine)
        return;

    for (const auto& actionType: m_registeredActionTypes)
        m_engine->unregisterAction(actionType);

    m_registeredActionTypes.clear();
}

} // namespace nx::vms::rules
