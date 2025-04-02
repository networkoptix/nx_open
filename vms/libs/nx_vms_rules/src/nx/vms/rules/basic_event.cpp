// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_event.h"

#include <QtCore/QMetaProperty>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/utils/event.h>

#include "aggregated_event.h"
#include "engine.h"
#include "strings.h"
#include "utils/event_details.h"
#include "utils/field.h"
#include "utils/type.h"

namespace nx::vms::rules {

BasicEvent::BasicEvent(std::chrono::microseconds timestamp, State state):
    m_timestamp(timestamp),
    m_state(state)
{
}

QString BasicEvent::type() const
{
    QString result = utils::type(metaObject());
    NX_ASSERT(!result.isEmpty(), "Event type is invalid");
    return result;
}

QString BasicEvent::subtype() const
{
    return {};
}

std::chrono::microseconds BasicEvent::timestamp() const
{
    return m_timestamp;
}

void BasicEvent::setTimestamp(const std::chrono::microseconds& timestamp)
{
    m_timestamp = timestamp;
}

State BasicEvent::state() const
{
    return m_state;
}

void BasicEvent::setState(State state)
{
    NX_ASSERT(state != State::none);
    m_state = state;
}

QString BasicEvent::cacheKey() const
{
    return {};
}

QVariantMap BasicEvent::details(
    common::SystemContext* context,
    const nx::vms::api::rules::PropertyMap& /*aggregatedInfo*/,
    Qn::ResourceInfoLevel detailLevel) const
{
    QVariantMap result;
    utils::insertIfNotEmpty(result, utils::kNameDetailName, name(context));
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName,
        extendedCaption(context, detailLevel));
    return result;
}

nx::vms::api::rules::PropertyMap BasicEvent::aggregatedInfo(const AggregatedEvent&) const
{
    return {};
}

QString BasicEvent::name(common::SystemContext* context) const
{
    return Strings::eventName(context, type());
}

QString BasicEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel /*detailLevel*/) const
{
    return name(context);
}

void BasicEvent::fillAggregationDetailsForServer(
    QVariantMap& details,
    common::SystemContext* context,
    nx::Uuid serverId,
    Qn::ResourceInfoLevel detailLevel,
    bool useAsSource)
{
    const auto server = context->resourcePool()->getResourceById(serverId);
    const auto serverName = server ? Strings::resource(server, detailLevel) : QString();

    details[utils::kAggregationKeyDetailName] = serverName;
    details[utils::kAggregationKeyDetailsDetailName] = Strings::resourceIp(server);
    details[utils::kAggregationKeyIconDetailName] = "server";

    if (useAsSource)
    {
        details[utils::kSourceResourcesTypeDetailName] = QVariant::fromValue(ResourceType::server);
        details[utils::kSourceTextDetailName] = serverName;
        details[utils::kSourceResourcesIdsDetailName] = QVariant::fromValue(UuidList({serverId}));
    }
}

void BasicEvent::fillAggregationDetailsForDevice(
    QVariantMap& details,
    common::SystemContext* context,
    nx::Uuid deviceId,
    Qn::ResourceInfoLevel detailLevel,
    bool useAsSource)
{
    const auto device = context->resourcePool()->getResourceById(deviceId);

    const auto deviceName = device ? Strings::resource(device, detailLevel) : QString();
    details[utils::kAggregationKeyDetailName] = deviceName;
    details[utils::kAggregationKeyDetailsDetailName] = Strings::resourceIp(device);
    details[utils::kAggregationKeyIconDetailName] = "camera";

    if (useAsSource)
    {
        details[utils::kSourceResourcesTypeDetailName] = QVariant::fromValue(ResourceType::device);
        details[utils::kSourceTextDetailName] = deviceName;
        details[utils::kSourceResourcesIdsDetailName] = QVariant::fromValue(UuidList({deviceId}));
    }
}

} // namespace nx::vms::rules
