// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metadata_types.h"

#include <nx/kit/debug.h>

namespace nx::sdk::analytics {

MetadataTypes::MetadataTypes():
    m_eventTypeIds(nx::sdk::makePtr<StringList>()),
    m_objectTypeIds(nx::sdk::makePtr<StringList>())
{
}

const IStringList* MetadataTypes::getEventTypeIds() const
{
    if (!NX_KIT_ASSERT(m_eventTypeIds))
        return nullptr;

    return shareToPtr(m_eventTypeIds).releasePtr();
}

const IStringList* MetadataTypes::getObjectTypeIds() const
{
    if (!NX_KIT_ASSERT(m_objectTypeIds))
        return nullptr;

    return shareToPtr(m_objectTypeIds).releasePtr();
}

bool MetadataTypes::isEmpty() const
{
    if (!NX_KIT_ASSERT(m_eventTypeIds && m_objectTypeIds))
        return true;

    return (m_eventTypeIds->count() == 0 && m_objectTypeIds->count() == 0);
}

void MetadataTypes::addEventTypeId(std::string eventTypeId)
{
    if (!NX_KIT_ASSERT(m_eventTypeIds))
        return;

    m_eventTypeIds->addString(std::move(eventTypeId));
}

void MetadataTypes::addObjectTypeId(std::string objectTypeId)
{
    if (!NX_KIT_ASSERT(m_objectTypeIds))
        return;

    m_objectTypeIds->addString(std::move(objectTypeId));
}

} // namespace nx::sdk::analytics
