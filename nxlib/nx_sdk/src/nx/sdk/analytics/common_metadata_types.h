#pragma once

#include <plugins/plugin_tools.h>

#include <nx/sdk/common_string_list.h>
#include <nx/sdk/analytics/metadata_types.h>

namespace nx {
namespace sdk {
namespace analytics {

class CommonMetadataTypes: public nxpt::CommonRefCounter<IMetadataTypes>
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual const IStringList* eventTypeIds() const override;
    virtual const IStringList* objectTypeIds() const override;

    // TODO: #mshevchenko: Rename to isEmpty().
    virtual bool isNull() const override;

    void addEventType(std::string eventTypeString);
    void addObjectType(std::string objectTypeString);

private:
    CommonStringList m_eventTypeList;
    CommonStringList m_objectTypeList;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
