// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "string_helper.h"

#include <algorithm>

#include <QtCore/QDateTime>

#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/time/formatter.h>

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

QString StringHelper::plugin(QnUuid pluginId) const
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

QString StringHelper::resource(QnUuid resourceId, Qn::ResourceInfoLevel detailLevel) const
{
    auto resource = systemContext()->resourcePool()->getResourceById(resourceId);
    if (!resource)
        return {};

    return QnResourceDisplayInfo(resource).toString(detailLevel);
}

QString StringHelper::resourceIp(QnUuid resourceId) const
{
    auto resource = systemContext()->resourcePool()->getResourceById(resourceId);
    if (!resource)
        return {};

    return QnResourceDisplayInfo(resource).host();
}

QString StringHelper::urlForCamera(
    QnUuid id,
    std::chrono::microseconds timestamp,
    bool usePublicIp,
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

        nx::utils::Url serverUrl = server->getApiUrl();
        if (usePublicIp)
        {
            const auto publicIp = server->getProperty(ResourcePropertyKey::Server::kPublicIp);
            if (publicIp.isEmpty())
                return {};

            const QStringList parts = publicIp.split(':');
            builder.setHost(parts[0]);
            if (parts.size() > 1)
                builder.setPort(parts[1].toInt());
            else
                builder.setPort(serverUrl.port(kDefaultHttpPort));
        }
        else /*usePublicIp*/
        {
            builder.setHost(serverUrl.host());
            builder.setPort(serverUrl.port(kDefaultHttpPort));
        }
    }

    builder.setPath("/static/index.html")
        .setFragment("/view/" + id.toSimpleString())
        .setQuery(QString("time=%1").arg(timestampMs.count()));

    const nx::utils::Url url = builder.toUrl();
    NX_ASSERT(url.isValid());
    return url.toWebClientStandardViolatingUrl();
}

} // namespace nx::vms::rules::utils
