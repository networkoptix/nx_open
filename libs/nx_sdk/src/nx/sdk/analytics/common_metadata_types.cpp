#include "common_metadata_types.h"

namespace nx {
namespace sdk {
namespace analytics {

void* CommonMetadataTypes::queryInterface(const nxpl::NX_GUID& interfaceId)
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

const IStringList* CommonMetadataTypes::eventTypeIds() const
{
    return &m_eventTypeList;
}

const IStringList* CommonMetadataTypes::objectTypeIds() const
{
    return &m_objectTypeList;
}

bool CommonMetadataTypes::isEmpty() const
{
    return (m_eventTypeList.count() == 0) && (m_objectTypeList.count() == 0);
}

void CommonMetadataTypes::addEventType(std::string eventTypeString)
{
    m_eventTypeList.addString(std::move(eventTypeString));
}

void CommonMetadataTypes::addObjectType(std::string objectTypeString)
{
    m_objectTypeList.addString(std::move(objectTypeString));
}

} // namespace analytics
} // namespace sdk
} // namespace nx
