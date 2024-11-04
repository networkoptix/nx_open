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

struct SubstitutionDoc
{
    QString text;
    QString resultExample;
    QString filterText = "All events.";
};

const QString kEventTimestampDescription = "The Current Epoch Unix Timestamp in %1.";
const QString kEventProlongedFilterText = "All prolonged events.";
const QString kEventTimeExample = "2021-01-13T09:11:23+00:00";
const QString kEventTimestampExample = "1644422205";
const QString kCustomTextExample = "Some custom text";
const QString kDeviceFilterText = "Events that include the <code>deviceIds</code> or "
                                  "<code>cameraId</code> field in their manifest.<br/>"
                                  "For example: Camera Disconnected Event, Network Issue.";

const SubstitutionDoc kDefaultDeviceIdDoc = {
    .text = "Internal id of the device which generated the event.",
    .resultExample = "00000000-0000-0000-0000-000000000000",
    .filterText = kDeviceFilterText};

const QString kDefaultDeviceNameText =
    "Name of the device which generated the event. For the Site events "
    "this will be the same, as <code>{site.name}</code>.";

const QString kDefaultDeviceNameExample = "Hallway camera";
const SubstitutionDoc kDefaultDeviceNameDoc = {.text = kDefaultDeviceNameText,
    .resultExample = kDefaultDeviceNameExample,
    .filterText = kDeviceFilterText};

const SubstitutionDoc kDefaultEventNameDoc = {
    .text = "Name of the event, that is used in Event rules, Event logs, etc."
            "<ul><li>For the Analytics Event this substituted "
            "with the <code>{event.type}</code> (Line Crossing, People Detection).</li>"
            "<li>For the Generic Event, this substituted with the event caption.</li></ul>",
    .resultExample = "Camera disconnected"};

const SubstitutionDoc kDefaultEventTypeDoc = {
    .text = "The analytics event types.", .resultExample = "Line Crossing"};

struct SubstitutionDesc
{
    FormatFunction formatFunction;
    bool visible = true;
    FilterFunction filter = {};
    SubstitutionDoc documentation = {};
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

} // namespace

struct EventParameterHelper::Private
{
    std::map<QString, SubstitutionDesc, nx::utils::CaseInsensitiveStringCompare> formatFunctions;
    FormatFunction formatFunction(
        const QString& name, common::SystemContext* context, const AggregatedEventPtr& event)
    {
        if (name.startsWith(kEventAttributesPrefix))
        {
            if (!isValidEventAttribute(name, context, event))
                return {};

            return
                [name](const AggregatedEventPtr& event, common::SystemContext* /*context*/)
                { return eventAttribute(name.sliced(kEventAttributesPrefix.size()), event); };
        }

        const auto it = formatFunctions.find(name);
        if (it == formatFunctions.end())
            return {};
        if (!substitutionIsApplicable(it->second, event->type(), event->state(), context))
            return {};
        return it->second.formatFunction;
    }

    void registerFormatFunction(const QString& parameter,const SubstitutionDesc& desc)
    {
        formatFunctions[parameter] = desc;
    }
    void initFormatFunctions();
};



void EventParameterHelper::Private::initFormatFunctions()
{
    registerFormatFunction("camera.id",
        {
            .formatFunction = &deviceId,
            .visible = false,
            .filter = &deviceEvents,
            .documentation = kDefaultDeviceIdDoc
        });

    registerFormatFunction("camera.name",
        {
            .formatFunction = &deviceName,
            .visible = false,
            .filter = &deviceEvents,
            .documentation =
            {
                .text = kDefaultDeviceNameText,
                .resultExample = kDefaultDeviceNameExample,
                .filterText = kDeviceFilterText
            }
        });

    registerFormatFunction("device.id",
        {
            .formatFunction = &deviceId,
            .visible = false,
            .documentation = kDefaultDeviceIdDoc
        });

    registerFormatFunction("device.ip",
        {
            .formatFunction = &deviceIp,
            .filter = &deviceEvents,
            .documentation =
            {
                .text = "IP address of the device which generated the event.",
                .resultExample = "192.168.10.3",
                .filterText = kDeviceFilterText
            }
        });

    registerFormatFunction("device.mac",
        {
            .formatFunction = &deviceMac,
            .filter = &deviceEvents,
            .documentation =
            {
                .text = "MAC address of the device which generated the event.",
                .resultExample = "00-09-18-52-1E-A9",
                .filterText = kDeviceFilterText
            }
        });

    registerFormatFunction(
        "device.name", {.formatFunction = &deviceName, .documentation = kDefaultDeviceNameDoc});

    registerFormatFunction("device.type",
        {
            .formatFunction = &deviceType,
            .documentation =
            {
                .text = "Type of the device which generated the event. "
                    "For the Site events this will be the Site name.",
                .resultExample = "Camera"
            }
        });

    registerFormatFunction("event.cameraId",
        {
            .formatFunction = &deviceId,
            .visible = false,
            .documentation = kDefaultDeviceIdDoc
        });

    registerFormatFunction("event.cameraName",
        {
            .formatFunction = &deviceName,
            .visible = false,
            .documentation = kDefaultDeviceNameDoc
        });

    registerFormatFunction("event.caption",
        {
            .formatFunction = &eventCaption,
            .documentation =
            {
                .text = "Generic event or analytics event caption (used in notifications). "
                    "Currently available to all events, but will return an empty string for them.",
                .resultExample = kCustomTextExample
            }
        });

    registerFormatFunction("event.description",
        {
            .formatFunction = &eventDescription,
            .documentation =
            {
                .text = "Generic event or analytics event description"
                    "(currently available to all events, but will return empty string).",
                .resultExample = kCustomTextExample
            }
        });

    registerFormatFunction("event.eventName",
        {
            .formatFunction = &eventName,
            .visible = false,
            .documentation = kDefaultEventNameDoc
        });

    registerFormatFunction("event.eventType",
        {
            .formatFunction = &eventType,
            .visible = false,
            .documentation = kDefaultEventTypeDoc
        });

    registerFormatFunction("event.extendedDescription",
        {.formatFunction = &extendedEventDescription, .visible = false});

    registerFormatFunction(
        "event.name", {.formatFunction = &eventName, .documentation = kDefaultEventNameDoc});

    registerFormatFunction("event.source",
        {
            .formatFunction = &eventSource,
            .documentation =
            {
                .text = "Event source. Can be manually filled for the generic or analytic events, "
                    "otherwise the source server name (for server-based events) "
                    "or camera name (for soft triggers) is used.",
                .resultExample = kCustomTextExample
            }
        });

    registerFormatFunction("event.time",
        {
            .formatFunction = &eventTime,
            .documentation =
            {
                .text = "Date and time of the event in ISO8601 format.",
                .resultExample = kEventTimeExample
            }
        });

    registerFormatFunction("event.time.start",
        {
            .formatFunction = &eventTimeStart,
            .filter = &prolongedEvents,
            .documentation =
             {
                .text = "Date and time of the event start in ISO8601 format.",
                .resultExample = kEventTimeExample,
                .filterText = kEventProlongedFilterText
            }
        });

    registerFormatFunction("event.time.end",
        {
            .formatFunction = &eventTimeEnd,
            .filter = &prolongedEvents,
            .documentation =
            {
                .text = "Date and time of the event end in ISO8601 format.",
                .resultExample = kEventTimeExample,
                .filterText = kEventProlongedFilterText
            }
        });

    registerFormatFunction("event.timestamp",
        {
            .formatFunction = &eventTimestamp<std::chrono::seconds>,
            .documentation =
            {
                .text = kEventTimestampDescription.arg("seconds"),
                .resultExample = kEventTimestampExample
            }
        });

    registerFormatFunction("event.timestampUs",
        {
            .formatFunction = &eventTimestamp<std::chrono::microseconds>,
            .documentation =
            {
                .text = kEventTimestampDescription.arg("microseconds"),
                .resultExample = kEventTimestampExample
            }
        });

    registerFormatFunction("event.timestampMs",
        {
            .formatFunction = &eventTimestamp<std::chrono::milliseconds>,
            .documentation =
            {
                .text = kEventTimestampDescription.arg("milliseconds"),
                .resultExample = kEventTimestampExample
            }
        });

    registerFormatFunction(
        "event.type", {.formatFunction = &eventType, .documentation = kDefaultEventTypeDoc});

    registerFormatFunction("server.name",
        {
            .formatFunction = &serverName,
            .documentation =
            {
                .text =
                    "Name of the server where the event was generated.",
                .resultExample = "RPI Server"
            }
        });

    registerFormatFunction("site.name",
        {
            .formatFunction = &siteName,
            .documentation =
            {
                .text = "Name of the Site where the event was generated.",
                .resultExample = "Burbank VMS"
            }
        });

    registerFormatFunction("user.name",
        {
            .formatFunction = &userName,
            .filter = &userEvents,
            .documentation =
            {
                .text = "Name of the user who triggered the event.",
                .resultExample = "admin",
                .filterText = "Events that include the <code>users</code> or <code>userId</code> "
                    "field in their manifest.<br/> For example: Soft trigger."
            }
        });
}

namespace nx::vms::rules::utils {

EventParameterHelper* EventParameterHelper::instance()
{
    static EventParameterHelper sInstance;
    return &sInstance;
}

EventParameterHelper::EventParameterHelper():
    d(new Private)
{
    d->initFormatFunctions();
}

EventParameterHelper::~EventParameterHelper()
{
}

QString EventParameterHelper::getHtmlDescription(bool skipHidden)
{
    QStringList htmlTable = {
        "<details>"
        "<summary>Event parameter can be one of the following values:</summary>"
        "<table>"
        "<tr>"
        "<th> Parameter </th>"
        "<th> Description </th>"
        "<th> Result example </th>"
        "<th> Applicable Events </th></tr>"};
    // OpenAPI documentation doesn't support full CSS styling.
    // To make table readable margins need to be added manually using spaces and tags.
    const QString row = "<tr><td>%1<br/><br/><br/></td>"
                        "<td>&nbsp;&nbsp%2<br/><br/><br/>"
                        "</td><td>&nbsp;&nbsp;%3<br/><br/><br/>"
                        "</td><td>&nbsp;&nbsp;%4<br/><br/><br/></td></tr>";
    for (auto& [key, desc]: d->formatFunctions)
    {
        if (!desc.visible && skipHidden)
            continue;

        const auto& description = desc.documentation;
        htmlTable.append(row.arg(addBrackets(key),
            description.text,
            description.resultExample,
            description.filterText));
    }
    htmlTable.append("</table></details>");
    return htmlTable.join("\n");
}

EventParameterHelper::EventParametersNames EventParameterHelper::getVisibleEventParameters(
    const QString& eventType,
    common::SystemContext* systemContext,
    const QString& objectTypeField,
    State eventState)
{
    if (eventType.isEmpty())
        return {};

    EventParametersNames result;
    for (auto& [key, desc]: d->formatFunctions)
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
    if (const auto function = d->formatFunction(eventParameter, context, eventAggregator))
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
