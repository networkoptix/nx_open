#pragma once

#include <plugins/plugin_tools.h>

#include <nx/sdk/common/string_list.h>
#include <nx/sdk/analytics/i_metadata_types.h>

namespace nx {
namespace sdk {
namespace analytics {
namespace common {

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
    nx::sdk::common::StringList m_eventTypeIds;
    nx::sdk::common::StringList m_objectTypeIds;
};

} // namespace common
} // namespace analytics
} // namespace sdk
} // namespace nx
