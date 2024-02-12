// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "string_helper.h"

#include <algorithm>

#include <QtCore/QDateTime>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
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

#include "../engine.h"
#include "../field_types.h"
#include "../manifest.h"

namespace {

QDateTime dateTimeFromMicroseconds(std::chrono::microseconds timestamp)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp.count() / 1000);
}

} // namespace

namespace nx::vms::rules::utils {

StringHelper::StringHelper(common::SystemContext* context): common::SystemContextAware(context)
{
}

QString StringHelper::timestamp(
    std::chrono::microseconds eventTimestamp,
    int eventCount,
    bool html) const
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

QString StringHelper::timestampDate(std::chrono::microseconds eventTimestamp) const
{
    const auto dateTime = dateTimeFromMicroseconds(eventTimestamp);
    return time::toString(dateTime.date());
}

QString StringHelper::timestampTime(std::chrono::microseconds eventTimestamp) const
{
    const auto dateTime = dateTimeFromMicroseconds(eventTimestamp);
    return time::toString(dateTime.time());
}

QString StringHelper::plugin(nx::Uuid pluginId) const
{
    using nx::analytics::taxonomy::AbstractState;
    using nx::analytics::taxonomy::AbstractEngine;

    if (pluginId.isNull())
        return {};

    const std::shared_ptr<AbstractState> taxonomyState = systemContext()->analyticsTaxonomyState();
    if (!taxonomyState)
        return {};

    const AbstractEngine* engineInfo = taxonomyState->engineById(pluginId.toString());
    if (engineInfo)
        return engineInfo->name();

    return {};
}

QString StringHelper::resource(nx::Uuid resourceId, Qn::ResourceInfoLevel detailLevel) const
{
    return resource(systemContext()->resourcePool()->getResourceById(resourceId), detailLevel);
}

QString StringHelper::resource(
    const QnResourcePtr& resource,
    Qn::ResourceInfoLevel detailLevel) const
{
    return resource ? QnResourceDisplayInfo(resource).toString(detailLevel) : QString();
}

QString StringHelper::resourceIp(nx::Uuid resourceId) const
{
    auto resource = systemContext()->resourcePool()->getResourceById(resourceId);
    if (!resource)
        return {};

    return QnResourceDisplayInfo(resource).host();
}

QString StringHelper::urlForCamera(
    nx::Uuid id,
    std::chrono::microseconds timestamp,
    Url urlType,
    const std::optional<nx::network::SocketAddress>& proxyAddress) const
{
    static constexpr int kDefaultHttpPort = 80;

    if (id.isNull())
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
        const auto camera =
            resourcePool()->getResourceById<QnVirtualCameraResource>(id);
        if (!camera)
            return {};

        auto server = camera->getParentServer();
        if (!server)
            return {};

        auto newServer =
            systemContext()->cameraHistoryPool()->getMediaServerOnTime(camera, timestampMs.count());
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
                const auto sId = nx::Uuid::fromStringSafe(systemSettings()->cloudSystemId());
                return builder.setHost(nx::network::SocketGlobals::cloud().cloudHost())
                    .setPath(NX_FMT("/systems/%1/view/%2", sId.toSimpleString(), id.toSimpleString()))
                    .setQuery(NX_FMT("time=%1", timestamp.count()))
                    .toUrl().toString();
            }
        };
    }

    builder.setPath("/static/index.html")
        .setFragment("/view/" + id.toSimpleString())
        .setQuery(QString("time=%1").arg(timestampMs.count()));

    const nx::utils::Url url = builder.toUrl();
    NX_ASSERT(url.isValid());
    return url.toWebClientStandardViolatingUrl();
}

QString StringHelper::subjects(const UuidSelection& selection) const
{
    if (selection.all)
        return tr("All users");

    // TODO: #amalov Remove dependency;
    nx::vms::event::StringsHelper oldHelper(systemContext());
    QnUserResourceList users;
    nx::vms::api::UserGroupDataList groups;
    nx::vms::common::getUsersAndGroups(systemContext(), selection.ids, users, groups);

    const int numDeleted = int(selection.ids.size()) - (users.size() + groups.size());
    NX_ASSERT(numDeleted >= 0);
    if (numDeleted <= 0)
        return oldHelper.actionSubjects(users, groups);

    const QString removedSubjectsText = tr("%n Removed subjects", "", numDeleted);
    return groups.empty() && users.empty()
        ? removedSubjectsText
        : oldHelper.actionSubjects(users, groups, false) + ", " + removedSubjectsText;
}

QString StringHelper::eventName(const QString& type) const
{
    if (const auto engine = systemContext()->vmsRulesEngine(); NX_ASSERT(engine))
    {
        if (const auto descriptor = engine->eventDescriptor(type))
            return descriptor->displayName;
    }

    return tr("Unknown event");
}

QString StringHelper::actionName(const QString& type) const
{
    if (const auto engine = systemContext()->vmsRulesEngine(); NX_ASSERT(engine))
    {
        if (const auto descriptor = engine->actionDescriptor(type))
            return descriptor->displayName;
    }

    return tr("Unknown action");
}

} // namespace nx::vms::rules::utils
