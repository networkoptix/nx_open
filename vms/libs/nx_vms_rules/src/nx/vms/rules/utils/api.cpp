// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "api.h"

#include <QtCore/QJsonArray>

#include <nx/network/http/custom_headers.h>
#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/action_info.h>
#include <nx/vms/api/rules/event_info.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/common/utils/schedule.h>

#include "../action_builder.h"
#include "../action_builder_field.h"
#include "../basic_action.h"
#include "../basic_event.h"
#include "../engine.h"
#include "../event_filter.h"
#include "../event_filter_field.h"
#include "../event_filter_fields/state_field.h"
#include "../rule.h"
#include "../strings.h"
#include "../utils/action.h"
#include "../utils/field_names.h"
#include "api_renamer.h"
#include "serialization.h"

namespace nx::vms::rules {

namespace {

const IdRenamer kIdRenamer;
const auto kPropertyOffset =
    QObject::staticMetaObject.propertyOffset() + QObject::staticMetaObject.propertyCount();

std::pair<QString, QJsonValue> toApi(QString name, const QJsonValue& value, int type)
{
    const auto converter = unitConverter(type);
    return {converter->nameToApi(std::move(name)), converter->valueToApi(value)};
}

int propertyType(const QMetaObject* meta, const QString& name)
{
    const auto index = meta->indexOfProperty(name.toUtf8().constData());
    NX_ASSERT(index, "Unregistered property type: %1", name);

    return meta->property(index).userType();
}

// Skip empty strings, arrays, and objects in the API output.
bool isRequiredInApi(const QJsonValue& value, bool allowEmptyArrays = false)
{
    if (value.isArray() && value.toArray().isEmpty())
        return allowEmptyArrays;

    if (value.isString() && value.toString().isEmpty())
        return false;

    if (value.isObject() && value.toObject().isEmpty())
        return false;

    return true;
}

std::pair<QString, QJsonValue> toApi(
    const QString& fieldName,
    const Field* field,
    bool allowEmptyArrays = false)
{
    auto propMap = serializeProperties(field, nx::utils::propertyNames(field));
    if (propMap.isEmpty())
        return {};

    if (propMap.size() == 1) // The field has single property.
    {
        const auto& propValue = propMap.first();

        if (!isRequiredInApi(propValue, allowEmptyArrays))
            return {};

        return toApi(fieldName, propValue, propertyType(field->metaObject(), propMap.firstKey()));
    }

    QJsonObject asObject;
    for (const auto& [name, value]: propMap.asKeyValueRange())
    {
        const auto converted = toApi(name, value, propertyType(field->metaObject(), name));
        if (isRequiredInApi(converted.second, allowEmptyArrays))
            asObject[converted.first] = converted.second;
    }

    return {kIdRenamer.toApi(fieldName), asObject};
}

template <class T>
bool fromApi(std::map<QString, QJsonValue>&& fieldMap, T* target, QString* error)
{
    for (auto [fieldName, field]: target->fields().asKeyValueRange())
    {
        if (!field->properties().visible)
            continue;

        const auto fieldMeta = field->metaObject();
        const auto propCount = fieldMeta->propertyCount();

        QMap<QString, QJsonValue> propMap;

        if (propCount - kPropertyOffset == 1) // The field has single property.
        {
            const auto propMeta = fieldMeta->property(kPropertyOffset);
            const auto converter = unitConverter(propMeta.userType());
            const auto apiFieldName = converter->nameToApi(fieldName);

            if (!fieldMap.contains(apiFieldName))
                continue;

            propMap[propMeta.name()] = converter->valueFromApi(fieldMap[apiFieldName]);
        }
        else
        {
            const auto apiFieldName = kIdRenamer.toApi(fieldName);

            if (!fieldMap.contains(apiFieldName))
                continue;

            const auto fieldValue = fieldMap[apiFieldName];

            if (!fieldValue.isObject())
            {
                if (error)
                    *error = format(Strings::tr("Field \"%1\" should be an object"), apiFieldName);

                return false;
            }

            const auto fieldObject = fieldValue.toObject();

            for (int i = kPropertyOffset; i < propCount; ++i)
            {
                const auto propMeta = fieldMeta->property(i);
                const auto converter = unitConverter(propMeta.userType());
                const auto apiPropName = converter->nameToApi(propMeta.name());

                if (!fieldObject.contains(apiPropName))
                    continue;

                propMap[propMeta.name()] = converter->valueFromApi(fieldObject[apiPropName]);
            }
        }

        if (!deserializeProperties(propMap, field))
        {
            if (error)
            {
                *error = format(Strings::tr(
                    "Unable to deserialize properties for field: %1"), fieldName);
            }

            return false;
        }
    }

    return true;
}

QString getRequestAuthorFromHeadersOrThrow(
    nx::vms::rules::Engine* engine, const nx::network::http::HttpHeaders& httpHeaders)
{
    const auto ruleIdIt = httpHeaders.find(Qn::CREATED_BY_RULE_HEADER_NAME);
    if (ruleIdIt == httpHeaders.end())
    {
        throw nx::network::rest::Exception::badRequest(
            nx::format(
                "Missed required `%1` header",
                Qn::CREATED_BY_RULE_HEADER_NAME));
    }

    const auto rule = engine->rule(nx::Uuid::fromString(ruleIdIt->second));
    if (!rule)
    {
        throw nx::network::rest::Exception::forbidden(
            nx::format(
                "Rule with `%1` id triggered the action is not found",
                ruleIdIt->second));
    }

    auto author = rule->author();
    if (author.isEmpty())
    {
        throw nx::network::rest::Exception::forbidden(
            nx::format(
                "Author is not provided for the rule with `%1` id triggered the action",
                ruleIdIt->second));
    }

    return author;
}

} // namespace

api::Rule serialize(const Rule* rule)
{
    NX_ASSERT(rule);

    api::Rule result;

    result.id = rule->id();
    result.author = rule->author();

    for (auto filter: rule->eventFilters())
    {
        result.eventList += serialize(filter);
    }
    for (auto builder: rule->actionBuilders())
    {
        result.actionList += serialize(builder);
    }
    result.comment = rule->comment();
    result.enabled = rule->enabled();
    result.internal = rule->isInternal();
    result.schedule = nx::vms::common::scheduleFromByteArray(rule->schedule());

    return result;
}

api::EventFilter serialize(const EventFilter* filter)
{
    NX_ASSERT(filter);

    api::EventFilter result;

    for (const auto& [name, field]: filter->fields().asKeyValueRange())
    {
        // State field must be processed even if it is not visible.
        if (!field->properties().visible && field->metatype() != fieldMetatype<StateField>())
            continue;

        api::Field serialized;
        serialized.type = field->metatype();
        serialized.props = field->serializedProperties();

        result.fields.emplace(name, std::move(serialized));
    }
    result.id = filter->id();
    result.type = filter->eventType();

    return result;
}

api::ActionBuilder serialize(const ActionBuilder* builder)
{
    NX_ASSERT(builder);

    api::ActionBuilder result;

    const auto fields = builder->fields();
    for (const auto& [name, field]: builder->fields().asKeyValueRange())
    {
        if (!field->properties().visible)
            continue;

        api::Field serialized;
        serialized.type = field->metatype();
        serialized.props = field->serializedProperties();

        result.fields.emplace(name, std::move(serialized));
    }
    result.id = builder->id();
    result.type = builder->actionType();

    return result;
}

api::ActionInfo serialize(const BasicAction* action, const QSet<QByteArray>& excludedProperties)
{
    auto props = nx::utils::propertyNames(action) - excludedProperties;

    return api::ActionInfo{
        .ruleId = action->ruleId(),
        .props = serializeProperties(action, props),
    };
}

api::EventInfo serialize(const BasicEvent* event, UuidList ruleIds)
{
    nx::vms::api::rules::EventInfo result;

    result.props = serializeProperties(event, nx::utils::propertyNames(event));

    for (auto id: ruleIds)
        result.triggeredRules.push_back(id);

    return result;
}

nx::vms::api::rules::RuleV4 toApi(
    const nx::vms::rules::Engine* engine, const nx::vms::api::rules::Rule& rule)
{
    return toApi(engine->buildRule(rule).get());
}

nx::vms::api::rules::RuleV4 toApi(const Rule* rule, bool allowEmptyArrays)
{
    nx::vms::api::rules::RuleV4 result;

    NX_ASSERT(!rule->isInternal());

    result.id = rule->id();
    result.enabled = rule->enabled();
    result.comment = rule->comment();
    result.schedule = nx::vms::common::scheduleFromByteArray(rule->schedule());

    const auto filter = rule->eventFilters().front();
    result.event["type"] = filter->eventType();

    for (const auto& [name, field]: filter->fields().asKeyValueRange())
    {
        if (!field->properties().visible)
            continue;

        if (auto eventField = toApi(name, field); !eventField.first.isEmpty())
            result.event.insert(std::move(eventField));
    }

    const auto builder = rule->actionBuilders().front();
    result.action["type"] = builder->actionType();

    for (const auto& [name, field]: builder->fields().asKeyValueRange())
    {
        if (!field->properties().visible)
            continue;

        if (auto actionField = toApi(name, field, allowEmptyArrays); !actionField.first.isEmpty())
            result.action.insert(std::move(actionField));
    }

    return result;
}

std::optional<nx::vms::api::rules::Rule> fromApi(
    const nx::vms::rules::Engine* engine,
    nx::vms::api::rules::RuleV4&& simple,
    QString author,
    QString* error)
{
    const auto eventType = simple.event["type"].toString();
    std::unique_ptr<EventFilter> filter;
    if (!engine->eventDescriptor(eventType) || !((filter = engine->buildEventFilter(eventType))))
    {
        if (error)
            *error = format(Strings::tr("Event: %1 is not registered"), eventType);

        return {};
    }

    const auto actionType = simple.action["type"].toString();
    std::unique_ptr<ActionBuilder> builder;
    if (!engine->actionDescriptor(actionType) || !((builder = engine->buildActionBuilder(actionType))))
    {
        if (error)
            *error = format(Strings::tr("Action: %1 is not registered"), actionType);

        return {};
    }

    if (!fromApi(std::move(simple.event), filter.get(), error) ||
        !fromApi(std::move(simple.action), builder.get(), error))
    {
        return {};
    }

    const auto stateField = filter->fieldByName<StateField>(utils::kStateFieldName);
    if (stateField && !stateField->properties().visible)
    {
        // This is the special case when the state filed (See `Soft Trigger` event) must be adjusted
        // accordingly the current action.
        if (nx::vms::rules::isProlonged(engine, builder.get()))
            stateField->setValue(vms::api::rules::State::none);
        else
            stateField->setValue(vms::api::rules::State::instant);
    }

    const auto rule = std::make_unique<Rule>(simple.id, engine);
    rule->setAuthor(std::move(author));
    rule->setEnabled(simple.enabled);
    rule->setComment(simple.comment);
    rule->setSchedule(nx::vms::common::scheduleToByteArray(simple.schedule));

    rule->addEventFilter(std::move(filter));
    rule->addActionBuilder(std::move(builder));

    if (const auto validationResult = rule->validity(engine->systemContext());
        validationResult.validity == QValidator::State::Invalid)
    {
        if (error)
            *error = format(Strings::tr("Rule validation failed: %1"), validationResult.description);

        return {};
    }

    return serialize(rule.get());
}

QString getRequestAuthorOrThrow(
    nx::vms::rules::Engine* engine, const nx::network::rest::Request& request)
{
    if (request.userSession.access.userId != nx::network::rest::kSystemAccess.userId)
        return request.userSession.session.userName;

    return getRequestAuthorFromHeadersOrThrow(engine, request.httpHeaders());
}

QString getRequestAuthorOrThrow(
    nx::vms::rules::Engine* engine, const nx::network::http::Request& request)
{
    return getRequestAuthorFromHeadersOrThrow(engine, request.headers);
}

} // namespace nx::vms::rules
