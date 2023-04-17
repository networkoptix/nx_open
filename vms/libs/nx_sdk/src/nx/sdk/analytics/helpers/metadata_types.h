// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_metadata_types.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/string_list.h>
#include <nx/sdk/ptr.h>

namespace nx::sdk::analytics {

class MetadataTypes: public RefCountable<IMetadataTypes>
{
public:
    MetadataTypes();

    virtual bool isEmpty() const override;

    void addEventTypeId(std::string eventTypeId);
    void addObjectTypeId(std::string objectTypeId);

protected:
    virtual const IStringList* getEventTypeIds() const override;
    virtual const IStringList* getObjectTypeIds() const override;

private:
    nx::sdk::Ptr<StringList> m_eventTypeIds;
    nx::sdk::Ptr<StringList> m_objectTypeIds;
};

} // namespace nx::sdk::analytics
