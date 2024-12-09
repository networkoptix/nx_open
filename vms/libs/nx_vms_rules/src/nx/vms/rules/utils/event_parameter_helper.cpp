// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameter_helper.h"

#include <QStringView>
#include <algorithm>

#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/utils.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/events/camera_disconnected_event.h>

#include "../aggregated_event.h"
#include "../engine.h"
#include "../group.h"
#include "event_parameter_format_functions.h"
#include "field.h"
#include "resource.h"
#include "type.h"

namespace {

using namespace nx::vms::rules::utils;
using namespace nx::vms::rules;
using namespace nx::vms;

constexpr char kStartOfSubstitutionSymbol = '{';
constexpr char kEndOfSubstitutionSymbol = '}';
constexpr char kGroupSeparatorSymbol = '.';
constexpr int kSubgroupStart = 2;

using FormatFunction = std::function<QString(SubstitutionContext*, common::SystemContext*)>;
using FilterFunction = std::function<bool(SubstitutionContext*, common::SystemContext*)>;

struct SubstitutionDoc
{
    QString text;
    QString resultExample;
    QString filterText = "All events.";
};

struct SubstitutionDesc
{
    FormatFunction formatFunction;
    bool visible = true;
    FilterFunction filter;
    SubstitutionDoc documentation;
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

EventParameterHelper::EventParametersNames getAttributesParameters(
    common::SystemContext*,
    const QString& objectTypeId,
    const QStringList& externallyCalculatedAttributes)
{
    if (objectTypeId.isEmpty())
        return {};

    EventParameterHelper::EventParametersNames result;

    for (const auto& attributeName: externallyCalculatedAttributes)
        result.insert(kEventAttributesPrefix + attributeName);

    // TODO: #vbutkevich. Change to collecting attributes by traversing the attributes of
    // objectType.
    return result;
}

bool isValidEventAttribute(SubstitutionContext* substitution, common::SystemContext*)
{
    if (!substitution->event)
        return false;

    using Attributes = nx::common::metadata::Attributes;

    const auto attributesVariant = substitution->event->property(kAttributesFieldName);
    if (!attributesVariant.canConvert<Attributes>())
        return false;

    const auto& attributes = attributesVariant.value<Attributes>();
    const auto attributeName = eventAttributeName(substitution);

    return std::ranges::any_of(
        attributes,
        [&attributeName](const auto& attribute)
        {
            return attributeName.compare(attribute.name, Qt::CaseInsensitive) == 0;
        });
}

bool deviceEvents(SubstitutionContext* context, common::SystemContext*)
{
    return !resourceField(context->manifest.value(), ResourceType::device).empty();
}

bool prolongedEvents(SubstitutionContext* context, common::SystemContext*)
{
    return context->state == State::started || context->state == State::stopped;
}

bool userEvents(SubstitutionContext* context, common::SystemContext*)
{
    return !resourceField(context->manifest.value(), ResourceType::user).empty();
}

bool hasProperty(SubstitutionContext* context, common::SystemContext*)
{
    return context->event && context->event->property(
        context->name.sliced(kEventFieldsPrefix.size()).toUtf8()).isValid();
}

} // namespace

struct EventParameterHelper::Private
{
    std::map<QString, SubstitutionDesc, nx::utils::CaseInsensitiveStringCompare> formatFunctions;

    const SubstitutionDesc* findDesc(const QString& name) const
    {
        auto it = formatFunctions.lower_bound(name);

        if (it != formatFunctions.end() && it->first == name)
            return &it->second;

        if (it != formatFunctions.begin())
        {
            --it;
            if (it->first.endsWith(kGroupSeparatorSymbol) && name.startsWith(it->first))
                return &it->second;
        }

        return nullptr;
    }

    FormatFunction formatFunction(
        common::SystemContext* system, SubstitutionContext* substitution) const
    {
        const auto desc = findDesc(substitution->name);
        if (!desc)
            return {};

        if (desc->filter && !desc->filter(substitution, system))
            return {};

        return desc->formatFunction;
    }

    void registerFormatFunction(const QString& parameter,const SubstitutionDesc& desc)
    {
        formatFunctions[parameter] = desc;
    }

    void initFormatFunctions();
};

void EventParameterHelper::Private::initFormatFunctions()
{
    registerFormatFunction(kEventAttributesPrefix,
        {
            .formatFunction = eventAttribute,
            .visible = true,
            .filter = isValidEventAttribute,
            .documentation = {
                .text = "Analytics Object attribute",
                .resultExample = "Blue",
                .filterText = "Analytic Object Event",
            }
        });

    registerFormatFunction(kEventFieldsPrefix,
    {
        .formatFunction = eventField,
        .visible = false,
        .filter = hasProperty,
        .documentation = {
            .text = "Event data field convertible to string",
            .resultExample = "6f9619ff-8b86-d011-b42d-00cf4fc964ff",
        }
    });

    registerFormatFunction(kEventDetailsPrefix,
    {
        .formatFunction = eventDetail,
        .visible = false,
        .filter = {},
        .documentation = {
            .text = "Event detail convertible to string",
            .resultExample = "Vehicle at Camera 1",
        }
    });

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
    const QString& objectTypeId,
    State eventState,
    const QStringList& attributes)
{
    auto context = SubstitutionContext{
        .manifest = systemContext->vmsRulesEngine()->eventDescriptor(eventType),
        .state = eventState,
        .objectTypeId = objectTypeId,
        .attributes = attributes,
    };

    if (!context.manifest)
        return {};

    EventParametersNames result;
    for (auto& [key, desc]: d->formatFunctions)
    {
        if (key.endsWith(kGroupSeparatorSymbol))
            continue; // TODO: #amalov Implement universal param name extraction.

        if (!desc.visible)
            continue; // Hidden elements are processed, but not visible in drop down menu.

        if (desc.filter && !desc.filter(&context, systemContext))
            continue;

        result.insert(key);
    }

    for (auto& attributeValue: getAttributesParameters(systemContext, objectTypeId, attributes))
        result.insert(attributeValue);

    return result;
}

QString EventParameterHelper::evaluateEventParameter(
    common::SystemContext* context,
    const AggregatedEventPtr& event,
    const QString& eventParameter) const
{
    auto substitution = SubstitutionContext{
        .name = eventParameter,
        .event = event,
        .manifest = context->vmsRulesEngine()->eventDescriptor(event->type()),
        .state = event->state(),
    };

    if (const auto function = d->formatFunction(context, &substitution))
        return function(&substitution, context);

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
