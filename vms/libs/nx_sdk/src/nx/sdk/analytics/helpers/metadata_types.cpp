#include "metadata_types.h"

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace analytics {

MetadataTypes::MetadataTypes():
    m_eventTypeIds(nx::sdk::makePtr<StringList>()),
    m_objectTypeIds(nx::sdk::makePtr<StringList>())
{
}

const IStringList* MetadataTypes::eventTypeIds() const
{
    if (!NX_KIT_ASSERT(m_eventTypeIds))
        return nullptr;

    m_eventTypeIds->addRef();
    return m_eventTypeIds.get();
}

const IStringList* MetadataTypes::objectTypeIds() const
{
    if (!NX_KIT_ASSERT(m_objectTypeIds))
        return nullptr;

    m_objectTypeIds->addRef();
    return m_objectTypeIds.get();
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

} // namespace analytics
} // namespace sdk
} // namespace nx
