#pragma once

#include <plugins/plugin_tools.h>

#include <nx/sdk/helpers/string_list.h>
#include <nx/sdk/analytics/i_metadata_types.h>

namespace nx {
namespace sdk {
namespace analytics {

class MetadataTypes: public nxpt::CommonRefCounter<IMetadataTypes>
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual const IStringList* eventTypeIds() const override;
    virtual const IStringList* objectTypeIds() const override;

    virtual bool isEmpty() const override;

    void addEventTypeId(std::string eventTypeId);
    void addObjectTypeId(std::string objectTypeId);

private:
    StringList m_eventTypeIds;
    StringList m_objectTypeIds;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
