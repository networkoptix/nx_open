#pragma once

#include <nx/sdk/helpers/ref_countable.h>

#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/string_list.h>
#include <nx/sdk/analytics/i_metadata_types.h>

namespace nx {
namespace sdk {
namespace analytics {

class MetadataTypes: public RefCountable<IMetadataTypes>
{
public:
    MetadataTypes();
    virtual const IStringList* eventTypeIds() const override;
    virtual const IStringList* objectTypeIds() const override;

    virtual bool isEmpty() const override;

    void addEventTypeId(std::string eventTypeId);
    void addObjectTypeId(std::string objectTypeId);

private:
    nx::sdk::Ptr<StringList> m_eventTypeIds;
    nx::sdk::Ptr<StringList> m_objectTypeIds;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
