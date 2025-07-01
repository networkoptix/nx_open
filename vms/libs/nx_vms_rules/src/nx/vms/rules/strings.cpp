// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "strings.h"

#include <algorithm>

#include <QtCore/QDateTime>

#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/time/formatter.h>

#include "aggregated_event.h"
#include "engine.h"
#include "field_types.h"
#include "manifest.h"
#include "utils/event_details.h"

namespace {

QDateTime dateTimeFromMicroseconds(std::chrono::microseconds timestamp)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp.count() / 1000);
}

} // namespace

namespace nx::vms::rules {

QString Strings::timestamp(
    std::chrono::microseconds eventTimestamp,
    int eventCount,
    bool html)
{
    const auto dateTime = dateTimeFromMicroseconds(eventTimestamp);

    if (html) {
        return eventCount > 1
            ? tr("%n times, first: %2 <b>%1</b>", "%1 means time, %2 means date", eventCount)
                .arg(time::toString(dateTime.time()))
                .arg(time::toString(dateTime.date()))
            : QString("%2 <b>%1</b>")
                .arg(time::toString(dateTime.time()))
                .arg(time::toString(dateTime.date()));
    }
    else
    {
        return eventCount > 1
            ? tr("First occurrence: %1 on %2 (%n times total)", "%1 means time, %2 means date", eventCount)
                .arg(time::toString(dateTime.time()))
                .arg(time::toString(dateTime.date()))
            : tr("Time: %1 on %2", "%1 means time, %2 means date")
                .arg(time::toString(dateTime.time()))
                .arg(time::toString(dateTime.date()));
    }
}

QString Strings::timestampDate(std::chrono::microseconds eventTimestamp)
{
    const auto dateTime = dateTimeFromMicroseconds(eventTimestamp);
    return time::toString(dateTime.date());
}

QString Strings::timestampTime(std::chrono::microseconds eventTimestamp)
{
    const auto dateTime = dateTimeFromMicroseconds(eventTimestamp);
    return time::toString(dateTime.time());
}

QString Strings::plugin(common::SystemContext* context, nx::Uuid pluginId)
{
    using nx::analytics::taxonomy::AbstractState;
    using nx::analytics::taxonomy::AbstractEngine;

    if (pluginId.isNull())
        return {};

    const std::shared_ptr<AbstractState> taxonomyState = context->analyticsTaxonomyState();
    if (!taxonomyState)
        return {};

    const AbstractEngine* engineInfo =
        taxonomyState->engineById(pluginId.toString(QUuid::WithBraces));
    if (engineInfo)
        return engineInfo->name();

    return {};
}

QString Strings::softTriggerName(const QString& name)
{
    auto result = name.trimmed();

    return result.isEmpty() ? tr("Soft Trigger") : result;
}

QString Strings::resource(
    common::SystemContext* context,
    nx::Uuid resourceId,
    Qn::ResourceInfoLevel detailLevel)
{
    return resource(context->resourcePool()->getResourceById(resourceId), detailLevel);
}

QString Strings::resource(
    const QnResourcePtr& resource,
    Qn::ResourceInfoLevel detailLevel)
{
    return resource ? QnResourceDisplayInfo(resource).toString(detailLevel) : QString();
}

QString Strings::removedResource(common::SystemContext* context,
    nx::vms::rules::ResourceType type)
{
    switch (type)
    {
        case ResourceType::user:
            return tr("Removed subject", "The subject is user or group");
        case ResourceType::device:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                context->resourcePool(), tr("Removed device"), tr("Removed camera"));
        case ResourceType::server:
            return tr("Removed server");
        case ResourceType::layout:
            return tr("Removed layout");
        default:
            NX_ASSERT(false, "Unexpected resource type: %1", type);
            return {};
    }
}

QString Strings::resourceIp(const QnResourcePtr& resource)
{
    return resource ? QnResourceDisplayInfo(resource).host() : QString();
}

QString Strings::eventExtendedDescription(
    AggregatedEventPtr event,
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel)
{
    static const QString kIndent = "  ";
    static const QString kSpacer = "...";
    static const QChar kLinesSeparator = '\n';

    QStringList result;

    auto details = event->details(context, detailLevel);
    result.push_back(details[utils::kExtendedCaptionDetailName].toString());

    auto addEvent =
        [context, &result, &detailLevel](const EventPtr& event)
        {
            result.push_back(Strings::timestampTime(event->timestamp()));
            auto eventDetails = event->details(context, detailLevel);
            auto detailing = eventDetails[utils::kDetailingDetailName]
                .template value<QStringList>();
            for (const auto& line: detailing)
                result.push_back(kIndent + line);
        };

    auto [firstEvents, lastEvents] = event->takeLimitedAmount();
    for (const auto& event: firstEvents)
        addEvent(event);

    if (event->count() > firstEvents.size())
    {
        result.push_back(kSpacer);
        for (const auto& event: lastEvents)
            addEvent(event);
        result.push_back({}); //< Add an empty line.
        result.push_back(Strings::totalNumberOfEvents(QString::number(event->count())));
    }
    return result.join(kLinesSeparator);
}

QString Strings::urlForCamera(
    const QnVirtualCameraResourcePtr& camera,
    std::chrono::microseconds timestamp,
    Url urlType,
    const std::optional<nx::network::SocketAddress>& proxyAddress)
{
    static constexpr int kDefaultHttpPort = 80;

    if (!camera)
        return {};

    const auto timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp);
    nx::network::url::Builder builder;
    builder.setScheme(nx::network::http::kSecureUrlSchemeName);

    if (proxyAddress) //< Client-side method to form camera links in the event log.
    {
        builder.setEndpoint(*proxyAddress);
    }
    else //< Server-side method to form links in emails.
    {
        auto server = camera->getParentServer();
        if (!server)
            return {};

        auto systemContext = camera->systemContext();

        auto newServer =
            systemContext->cameraHistoryPool()->getMediaServerOnTime(camera, timestampMs.count());
        if (newServer)
            server = newServer;

        nx::Url serverUrl = server->getRemoteUrl();
        switch (urlType)
        {
            case Url::localIp:
                builder.setHost(serverUrl.host());
                builder.setPort(serverUrl.port(kDefaultHttpPort));
                break;
            case Url::publicIp:
            {
                const auto publicIp = server->getProperty(ResourcePropertyKey::Server::kPublicIp);
                if (publicIp.isEmpty())
                    return QString();

                const QStringList parts = publicIp.split(':');
                builder.setHost(parts[0]);
                if (parts.size() > 1)
                    builder.setPort(parts[1].toInt());
                else
                    builder.setPort(serverUrl.port(kDefaultHttpPort));
                break;
            }
            case Url::cloud:
            {
                const auto cloudSystemId = nx::Uuid::fromStringSafe(
                    systemContext->globalSettings()->cloudSystemId());
                return builder.setHost(nx::network::SocketGlobals::cloud().cloudHost())
                    .setPath(NX_FMT("/systems/%1/view/%2",
                        cloudSystemId.toSimpleString(),
                        camera->getId().toSimpleString()))
                    .setQuery(NX_FMT("time=%1", timestampMs.count()))
                    .toUrl().toString();
            }
        };
    }

    builder.setPath("/static/index.html")
        .setFragment("/view/" + camera->getId().toSimpleString())
        .setQuery(QString("time=%1").arg(timestampMs.count()));

    const nx::Url url = builder.toUrl();
    NX_ASSERT(url.isValid());
    return url.toWebClientStandardViolatingUrl();
}

QString Strings::eventName(Engine* engine, const QString& type)
{
    if (NX_ASSERT(engine))
    {
        if (const auto descriptor = engine->eventDescriptor(type))
            return descriptor->displayName;
    }

    return tr("Unknown event");
}

QString Strings::eventName(common::SystemContext* context, const QString& type)
{
    return eventName(context->vmsRulesEngine(), type);
}

QStringList Strings::eventDetails(const QVariantMap& details)
{
    return details.value(utils::kDetailingDetailName).toStringList();
}

QString Strings::actionName(Engine* engine, const QString& type)
{
    if (NX_ASSERT(engine))
    {
        if (const auto descriptor = engine->actionDescriptor(type))
            return descriptor->displayName;
    }

    return tr("Unknown action");
}

QString Strings::actionName(common::SystemContext* context, const QString& type)
{
    return actionName(context->vmsRulesEngine(), type);
}

QString Strings::anyEvent()
{
    return tr("Any event");
}

QString Strings::analyticsEvents()
{
    return tr("Analytics events");
}

QString Strings::anyAnalyticsEvent()
{
    return tr("Any analytics event");
}

QString Strings::cameraIssues_internal()
{
    return tr("Camera issues");
}

QString Strings::deviceIssues_internal()
{
    return tr("Device issues");
}

QString Strings::deviceIssues(common::SystemContext* context)
{
    return QnDeviceDependentStrings::getDefaultNameFromSet(
        context->resourcePool(),
        deviceIssues_internal(),
        cameraIssues_internal());
}

QString Strings::anyCameraIssue_internal()
{
    return tr("Any camera issue");
}

QString Strings::anyDeviceIssue_internal()
{
    return tr("Any device issue");
}

QString Strings::anyDeviceIssue(common::SystemContext* context)
{
    return QnDeviceDependentStrings::getDefaultNameFromSet(
        context->resourcePool(),
        anyDeviceIssue_internal(),
        anyCameraIssue_internal());
}

QString Strings::serverEvents()
{
    return tr("Server events");
}

QString Strings::anyServerEvent()
{
    return tr("Any server event");
}

QString Strings::invalidFieldType()
{
    return tr("Invalid field type is provided");
}

QString Strings::unexpectedPolicy()
{
    return tr("Unexpected validation policy");
}

QString Strings::selectServer()
{
    return tr("Select at least one Server");
}

QString Strings::selectUser()
{
    return tr("Select at least one user");
}

QString Strings::selectCamera(common::SystemContext* context, bool allowMultipleSelection)
{
    if (!allowMultipleSelection)
        return tr("Select exactly one camera");

    return QnDeviceDependentStrings::getDefaultNameFromSet(
        context->resourcePool(),
        tr("Select at least one device"),
        tr("Select at least one camera"));
}

QString Strings::noSuitableServers(QValidator::State state)
{
    return state == QValidator::State::Intermediate
        ? tr("Not all servers are suitable")
        : tr("There are no suitable servers");
}

QString Strings::fieldValueMustBeProvided(const QString& fieldName)
{
    return tr(
        "Field %1 value must be provided for the given validation policy",
        "API error message when event rule cannot be created due to incomplete fields set")
        .arg(fieldName);
}

QString Strings::layoutCanOnlyBeShownToOwner(const QString& ownerName)
{
    return tr("Chosen local layout can only be shown to its owner %1").arg(ownerName);
}

QString Strings::usersHaveNoAccessToLayout(bool allUsersWithoutAccess)
{
    return allUsersWithoutAccess
        ? tr("None of selected users have access to the selected layout")
        : tr("Some users do not have access to the selected layout");
}

QString Strings::camerasWereRemoved(common::SystemContext* context, int count)
{
    return QnDeviceDependentStrings::getDefaultNameFromSet(
        context->resourcePool(),
        tr("Selected devices were removed", "", count),
        tr("Selected cameras were removed", "", count));
}

QString Strings::serversWereRemoved(int count)
{
    return tr("Selected servers were removed", "", count);
}

QString Strings::layoutsWereRemoved(int count)
{
    return tr("Selected layouts were removed", "", count);
}

QString Strings::negativeTime()
{
    return tr("Time value cannot be less than zero");
}

QString Strings::negativeDuration()
{
    return tr("Duration cannot be less than zero");
}

QString Strings::invalidDuration(
    std::chrono::microseconds value, std::chrono::seconds min, std::chrono::seconds max)
{
    if (value < min)
    {
        return tr("Value cannot be less than %1").arg(toString(min));
    }

    if (value > max)
    {
        return tr("Value cannot be more than %1").arg(toString(max));
    }

    return {};
}

QString Strings::nameAndValue(const QString& name, const QString& value)
{
    return value.isEmpty()
        ? name
        : name + ": " + value;
}

QString Strings::caption(const QString& value)
{
    return nameAndValue(tr("Caption"), value);
}

QString Strings::description(const QString& value)
{
    return nameAndValue(tr("Description"), value);
}

QString Strings::source(const QString& value)
{
    return nameAndValue(tr("Source"), value);
}

QString Strings::totalNumberOfEvents(const QString& value)
{
    return nameAndValue(tr("Total number of events"), value);
}

QString Strings::selectIntegrationAction()
{
    return tr("Select Integration Action");
}

QString Strings::integrationNotFoundForIntegrationAction(const QString& integrationAction)
{
    return tr("Integration not found for Integration Action %1").arg(integrationAction);
}

QString Strings::complexJsonValueType(const QString& key)
{
    return tr("Unsupported JSON value type in key %1").arg(key);
}

TranslatableString Strings::at()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("At"));
}

TranslatableString Strings::to()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("To"));
}

TranslatableString Strings::toUsers()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("To Users"));
}

TranslatableString Strings::for_()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("For"));
}

TranslatableString Strings::occursAt()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Occurs At"));
}

TranslatableString Strings::beginWhen()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Begin When"));
}

TranslatableString Strings::duration()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Duration"));
}

TranslatableString Strings::preRecording()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Pre-Recording"));
}

TranslatableString Strings::postRecording()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Post-Recording"));
}

TranslatableString Strings::intervalOfAction()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Interval of Action"));
}

TranslatableString Strings::state()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("State"));
}

TranslatableString Strings::volume()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Volume"));
}

TranslatableString Strings::rewind()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Rewind"));
}

TranslatableString Strings::onLayout()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("On Layout"));
}

TranslatableString Strings::eventDevices()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Event Devices"));
}

TranslatableString Strings::ofType()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Of Type"));
}

TranslatableString Strings::andCaption()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("And Caption"));
}

TranslatableString Strings::andDescription()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("And Description"));
}

TranslatableString Strings::method()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Method"));
}

} // namespace nx::vms::rules
