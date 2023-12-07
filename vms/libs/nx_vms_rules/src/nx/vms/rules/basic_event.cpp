// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_event.h"

#include <QtCore/QMetaProperty>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/common/system_context.h>

#include "engine.h"
#include "utils/event_details.h"
#include "utils/field.h"
#include "utils/string_helper.h"
#include "utils/type.h"

namespace nx::vms::rules {

BasicEvent::BasicEvent(std::chrono::microseconds timestamp, State state):
    m_timestamp(timestamp),
    m_state(state)
{
}

QString BasicEvent::type() const
{
    return utils::type(metaObject()); //< Assert?
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

QString BasicEvent::uniqueName() const
{
    return type();
}

QString BasicEvent::aggregationKey() const
{
    return resourceKey();
}

QString BasicEvent::cacheKey() const
{
    return {};
}

QString BasicEvent::source(common::SystemContext* context) const
{
    if (auto resource = context->resourcePool()->getResourceById(sourceId()))
        return resource->getName();

    return sourceId().toString();
}

QVariantMap BasicEvent::details(common::SystemContext* context) const
{
    QVariantMap result;

    utils::insertIfNotEmpty(result, utils::kTypeDetailName, type());
    utils::insertIfNotEmpty(result, utils::kNameDetailName, name(context));
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));

    if (const auto source = sourceId(); !source.isNull())
    {
        result[utils::kSourceIdDetailName] = QVariant::fromValue(source);
        utils::insertIfNotEmpty(
            result,
            utils::kSourceNameDetailName,
            utils::StringHelper(context).resource(source));
    }

    return result;
}

QString BasicEvent::name(common::SystemContext* context) const
{
    return utils::StringHelper(context).eventName(type());
}

QString BasicEvent::extendedCaption(common::SystemContext* context) const
{
    return tr("%1 event has occurred").arg(name(context));
}

QnUuid BasicEvent::sourceId() const
{
    const auto getId = [this](const char* propName){ return property(propName).value<QnUuid>(); };

    if (const auto cameraId = getId(utils::kCameraIdFieldName); !cameraId.isNull())
        return cameraId;

    if (const auto serverId = getId(utils::kServerIdFieldName); !serverId.isNull())
        return serverId;

    return {};
}

} // namespace nx::vms::rules
