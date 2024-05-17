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
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/time/formatter.h>

#include "engine.h"
#include "field_types.h"
#include "manifest.h"

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

    const AbstractEngine* engineInfo = taxonomyState->engineById(pluginId.toString());
    if (engineInfo)
        return engineInfo->name();

    return {};
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

QString Strings::resourceIp(const QnResourcePtr& resource)
{
    return resource ? QnResourceDisplayInfo(resource).host() : QString();
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

        nx::utils::Url serverUrl = server->getRemoteUrl();
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
                    .setQuery(NX_FMT("time=%1", timestamp.count()))
                    .toUrl().toString();
            }
        };
    }

    builder.setPath("/static/index.html")
        .setFragment("/view/" + camera->getId().toSimpleString())
        .setQuery(QString("time=%1").arg(timestampMs.count()));

    const nx::utils::Url url = builder.toUrl();
    NX_ASSERT(url.isValid());
    return url.toWebClientStandardViolatingUrl();
}

QString Strings::subjects(common::SystemContext* context, const UuidSelection& selection)
{
    if (selection.all)
        return tr("All users");

    // TODO: #amalov Remove dependency;
    nx::vms::event::StringsHelper oldHelper(context);
    QnUserResourceList users;
    nx::vms::api::UserGroupDataList groups;
    nx::vms::common::getUsersAndGroups(context, selection.ids, users, groups);

    const int numDeleted = int(selection.ids.size()) - (users.size() + groups.size());
    NX_ASSERT(numDeleted >= 0);
    if (numDeleted <= 0)
        return oldHelper.actionSubjects(users, groups);

    const QString removedSubjectsText = tr("%n Removed subjects", "", numDeleted);
    return groups.empty() && users.empty()
        ? removedSubjectsText
        : oldHelper.actionSubjects(users, groups, false) + ", " + removedSubjectsText;
}

QString Strings::eventName(common::SystemContext* context, const QString& type)
{
    if (const auto engine = context->vmsRulesEngine(); NX_ASSERT(engine))
    {
        if (const auto descriptor = engine->eventDescriptor(type))
            return descriptor->displayName;
    }

    return tr("Unknown event");
}

QString Strings::actionName(common::SystemContext* context, const QString& type)
{
    if (const auto engine = context->vmsRulesEngine(); NX_ASSERT(engine))
    {
        if (const auto descriptor = engine->actionDescriptor(type))
            return descriptor->displayName;
    }

    return tr("Unknown action");
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

TranslatableString Strings::at()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("At"));
}

TranslatableString Strings::to()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("To"));
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

TranslatableString Strings::fixedDuration()
{
    return NX_DYNAMIC_TRANSLATABLE(tr("Fixed duration"));
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

} // namespace nx::vms::rules
