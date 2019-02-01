#include "metadata_types.h"

namespace nx {
namespace sdk {
namespace analytics {

void* MetadataTypes::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return this;
    }

    if (interfaceId == IID_MetadataTypes)
    {
        addRef();
        return this;
    }

    return nullptr;
}

const IStringList* MetadataTypes::eventTypeIds() const
{
    return &m_eventTypeIds;
}

const IStringList* MetadataTypes::objectTypeIds() const
{
    return &m_objectTypeIds;
}

bool MetadataTypes::isEmpty() const
{
    return (m_eventTypeIds.count() == 0) && (m_objectTypeIds.count() == 0);
}

void MetadataTypes::addEventTypeId(std::string eventTypeId)
{
    m_eventTypeIds.addString(std::move(eventTypeId));
}

void MetadataTypes::addObjectTypeId(std::string objectTypeId)
{
    m_objectTypeIds.addString(std::move(objectTypeId));
}

} // namespace analytics
} // namespace sdk
} // namespace nx
