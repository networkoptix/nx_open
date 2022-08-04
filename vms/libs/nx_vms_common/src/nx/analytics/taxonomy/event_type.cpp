// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_type.h"

#include <nx/analytics/taxonomy/base_object_event_type_impl.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

EventType::EventType(EventTypeDescriptor eventTypeDescriptor, QObject* parent):
    AbstractEventType(parent),
    m_impl(std::make_shared<
        BaseObjectEventTypeImpl<EventTypeDescriptor, AbstractEventType, EventType>>(
            eventTypeDescriptor,
            kEventTypeDescriptorTypeName,
            /*owner*/ this))
{
}

QString EventType::id() const
{
    return m_impl->id();
}

QString EventType::name() const
{
    return m_impl->name();
}

QString EventType::icon() const
{
    return m_impl->icon();
}

nx::analytics::taxonomy::AbstractEventType* EventType::base() const
{
    return m_impl->base();
}

std::vector<AbstractEventType*> EventType::derivedTypes() const
{
    return m_impl->derivedTypes();
}

std::vector<AbstractAttribute*> EventType::baseAttributes() const
{
    return m_impl->baseAttributes();
}

std::vector<AbstractAttribute*> EventType::ownAttributes() const
{
    return m_impl->ownAttributes();
}

std::vector<AbstractAttribute*> EventType::attributes() const
{
    return m_impl->attributes();
}

std::vector<AbstractAttribute*> EventType::supportedAttributes() const
{
    return m_impl->supportedAttributes();
}

std::vector<AbstractAttribute*> EventType::supportedOwnAttributes() const
{
    return m_impl->supportedOwnAttributes();
}

bool EventType::isStateDependent() const
{
    return m_impl->descriptor().flags.testFlag(EventTypeFlag::stateDependent);
}

bool EventType::isRegionDependent() const
{
    return m_impl->descriptor().flags.testFlag(EventTypeFlag::regionDependent);
}

bool EventType::isHidden() const
{
    return m_impl->descriptor().flags.testFlag(EventTypeFlag::hidden);
}

bool EventType::useTrackBestShotAsPreview() const
{
    return m_impl->descriptor().flags.testFlag(EventTypeFlag::useTrackBestShotAsPreview);
}

bool EventType::hasEverBeenSupported() const
{
    return m_impl->hasEverBeenSupported();
}

bool EventType::isPrivate() const
{
    return m_impl->isPrivate();
}

std::vector<AbstractScope*> EventType::scopes() const
{
    return m_impl->scopes();
}

nx::vms::api::analytics::EventTypeDescriptor EventType::serialize() const
{
    return m_impl->serialize();
}

void EventType::addDerivedType(AbstractEventType* derivedEventType)
{
    m_impl->addDerivedType(derivedEventType);
}

void EventType::resolvePrivateness(bool hasPublicDescendants)
{
    m_impl->resolvePrivateness(hasPublicDescendants);
}

void EventType::resolvePrivateness()
{
    m_impl->resolvePrivateness();
}

void EventType::resolveScopes(InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    m_impl->resolveScopes(inOutInternalState, errorHandler);
}

void EventType::resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    m_impl->resolve(inOutInternalState, errorHandler);
}

void EventType::resolveSupportedAttributes(
    InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    m_impl->resolveSupportedAttributes(inOutInternalState, errorHandler);
}

} // namespace nx::analytics::taxonomy
