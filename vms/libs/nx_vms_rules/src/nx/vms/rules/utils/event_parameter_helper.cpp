// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameter_helper.h"

#include <QStringView>

#include <QtCore/QStringLiteral>

#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/utils.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/events/camera_disconnected_event.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/group.h>
#include <nx/vms/rules/utils/event_parameter_format_functions.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>

namespace {

using namespace nx::vms::rules::utils;
using namespace nx::vms::rules;
using namespace nx::vms;

constexpr char kStartOfSubstitutionSymbol = '{';
constexpr char kEndOfSubstitutionSymbol = '}';
constexpr char kGroupSeparatorSymbol = '.';
constexpr int kSubgroupStart = 2;
const auto kEventAttributesPrefix = QStringLiteral("event.attributes.");

using FormatFunction = std::function<QString(const AggregatedEventPtr&, common::SystemContext*)>;
using FilterFunction = std::function<bool(const ItemDescriptor&, State)>;

struct SubstitutionDesc
{
    FormatFunction formatFunction;
    bool visible = true;
    FilterFunction filter = {};
};

EventParameterHelper::EventParametersNames getAttributesParameters(
    const QString& eventType, common::SystemContext* systemContext, const QString& objectId)
{
    if (eventType != type<AnalyticsObjectEvent>() || objectId.isEmpty())
        return {};

    EventParameterHelper::EventParametersNames result;
    const auto attributesNames =
        getAttributesNames(systemContext->analyticsTaxonomyState().get(), objectId);
    for (auto& attribute: attributesNames)
        result.insert(kEventAttributesPrefix + attribute);
    return result;
}

bool isValidEventAttribute(
    const QString& text,
    common::SystemContext* systemContext,
    const AggregatedEventPtr& eventAggregator)
{
    if (systemContext->analyticsTaxonomyState()->objectTypes().empty())
    {
        // Taxonomy state is not initialized. Check that event at least has property attributes.
        return eventAggregator->initialEvent()->property(kAttributesFieldName).isValid();
    }

    const auto objectTypeId = eventAggregator->initialEvent()->property(kObjectTypeIdFieldName);
    if (!objectTypeId.isValid() || !objectTypeId.canConvert<QString>())
        return false;
    return getAttributesParameters(eventAggregator->type(), systemContext, objectTypeId.toString())
        .contains(text);
}

bool deviceEvents(const ItemDescriptor& itemDescriptor, State)
{
    return std::any_of(itemDescriptor.fields.begin(),itemDescriptor.fields.end(),
        [](const FieldDescriptor& desc)
        {
            return desc.fieldName == kDeviceIdsFieldName || desc.fieldName == kCameraIdFieldName;\
        });
}

bool prolongedEvents(const ItemDescriptor&, State eventState)
{
    return eventState == State::started || eventState == State::stopped;
}

bool userEvents(const ItemDescriptor& itemDescriptor, State)
{
    return std::any_of(itemDescriptor.fields.begin(),itemDescriptor.fields.end(),
        [](const FieldDescriptor& desc)
        { return desc.fieldName == kUsersFieldName || desc.fieldName == kUserIdFieldName; });
}

bool substitutionIsApplicable(
    const SubstitutionDesc& desc,
    const QString& eventType,
    State eventState,
    common::SystemContext* context)
{
    if (!desc.filter)
        return true;

    const auto eventDesc = context->vmsRulesEngine()->eventDescriptor(eventType);
    if (!eventDesc)
        return false;

    return desc.filter(eventDesc.value(), eventState);
}

const std::map<QString, SubstitutionDesc, nx::utils::CaseInsensitiveStringCompare> kFormatFunctions = {
    {"camera.id", {&deviceId, false, &deviceEvents}},
    {"camera.name", {&deviceName, false, &deviceEvents}},
    {"device.id", {&deviceId, false}},
    {"device.ip", {&deviceIp, true, &deviceEvents}},
    {"device.mac", {&deviceMac, true, &deviceEvents}},
    {"device.name", {&deviceName}},
    {"device.type", {&deviceType}},
    {"event.cameraId", {&deviceId, false}},
    {"event.cameraName", {&deviceName, false}},
    {"event.caption", {&eventCaption}},
    {"event.description", {&eventDescription}},
    {"event.eventName", {&eventName, false}},
    {"event.eventType", {&eventType, false}},
    {"event.extendedDescription", {&extendedEventDescription, false}},
    {"event.name", {&eventName}},
    {"event.source", {&eventSource}},
    {"event.time", {&eventTime}},
    {"event.time.start", {&eventTimeStart, true, &prolongedEvents}},
    {"event.time.end", {&eventTimeEnd, true, &prolongedEvents}},
    {"event.timestamp", {&eventTimestamp<std::chrono::seconds>}},
    {"event.timestampUs", {&eventTimestamp<std::chrono::microseconds>}},
    {"event.timestampMs", {&eventTimestamp<std::chrono::milliseconds>}},
    {"event.type", {&eventType}},
    {"server.name", {&serverName}},
    {"site.name", {&siteName}},
    {"user.name", {&userName, true, &userEvents}},
};

FormatFunction formatFunction(
    const QString& name, common::SystemContext* context, const AggregatedEventPtr& event)
{
    if (name.startsWith(kEventAttributesPrefix))
    {
        if (!isValidEventAttribute(name, context, event))
            return {};

        return [name](const AggregatedEventPtr& event, common::SystemContext* /*context*/)
               { return eventAttribute(name.sliced(kEventAttributesPrefix.size()), event); };
    }

    const auto it = kFormatFunctions.find(name);
    if (it == kFormatFunctions.end())
        return {};
    if (!substitutionIsApplicable(it->second, event->type(), event->state(), context))
        return {};
    return it->second.formatFunction;
}

} // namespace

namespace nx::vms::rules::utils {
EventParameterHelper::EventParametersNames EventParameterHelper::getVisibleEventParameters(
    const QString& eventType,
    common::SystemContext* systemContext,
    const QString& objectTypeField,
    State eventState)
{
    if (eventType.isEmpty())
        return {};

    EventParametersNames result;
    for (auto& [key, desc]: kFormatFunctions)
    {
        if (!desc.visible)
            continue; // Hidden elements are processed, but not visible in drop down menu.

        if (!substitutionIsApplicable(desc, eventType, eventState, systemContext))
            continue;

        result.insert(key);
    }

    for (auto& attributeValue: getAttributesParameters(eventType, systemContext, objectTypeField))
        result.insert(attributeValue);

    return result;
}

QString EventParameterHelper::evaluateEventParameter(
    common::SystemContext* context, const AggregatedEventPtr& eventAggregator, const QString& eventParameter)
{
    if (const auto function = formatFunction(eventParameter, context, eventAggregator))
        return function(eventAggregator, context);

    const auto propertyValue = eventAggregator->property(eventParameter.toUtf8().data());
    if (propertyValue.isValid() && propertyValue.canConvert<QString>())
    {
        // Found a valid event field, use it instead of the placeholder.
        return propertyValue.toString();
    }
    // Event parameter not found. Just add placeholder name to the result text.
    return addBrackets(eventParameter);
}

QString EventParameterHelper::addBrackets(const QString& text)
{
    return kStartOfSubstitutionSymbol + text + kEndOfSubstitutionSymbol;
}

QString EventParameterHelper::removeBrackets(QString text)
{
    if (!text.isEmpty() && text.front() == kStartOfSubstitutionSymbol)
        text.removeFirst();
    if (!text.isEmpty() && text.back() == kEndOfSubstitutionSymbol)
        text.removeLast();
    return text;
}


QString EventParameterHelper::getMainGroupName(const QString& text)
{
    auto wordWithoutBrackets = removeBrackets(text);

    auto groups = wordWithoutBrackets.split(kGroupSeparatorSymbol);
    if (groups.empty())
        return wordWithoutBrackets;
    return groups[0];
}

QString EventParameterHelper::getSubGroupName(const QString& text)
{
    auto wordWithoutBrackets = removeBrackets(text);

    auto groups = wordWithoutBrackets.split(kGroupSeparatorSymbol);
    if (groups.empty())
        return wordWithoutBrackets;

    if (groups.size() > kSubgroupStart)
        return groups[0] + kGroupSeparatorSymbol + groups[1];
    return wordWithoutBrackets;
}

bool EventParameterHelper::isStartOfEventParameter(const QChar& symbol)
{
    return symbol == kStartOfSubstitutionSymbol;
}

bool EventParameterHelper::isEndOfEventParameter(const QChar& symbol)
{
    return symbol == kEndOfSubstitutionSymbol;
}

QChar EventParameterHelper::startCharOfEventParameter()
{
    return kStartOfSubstitutionSymbol;
}

QChar EventParameterHelper::endCharOfEventParameter()
{
    return kEndOfSubstitutionSymbol;
}

int EventParameterHelper::getLatestEventParameterPos(const QStringView& text, int stopPosition)
{
    if (stopPosition >= text.size())
        return -1;

    for (int i = stopPosition; i >= 0; i--)
    {
        if (text[i] == kStartOfSubstitutionSymbol)
            return i;
        if (text[i] == kEndOfSubstitutionSymbol)
            return -1;
    }
    return -1;
}

bool EventParameterHelper::isIncompleteEventParameter(const QString& word)
{
    return word.startsWith(kStartOfSubstitutionSymbol) && !word.endsWith(kEndOfSubstitutionSymbol);
}

bool EventParameterHelper::containsSubgroups(const QString& eventParameter)
{
    return eventParameter.split(kGroupSeparatorSymbol).size() > kSubgroupStart;
}

} // namespace nx::vms::rules::utils
