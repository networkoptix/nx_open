// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/i_timestamped_object_metadata.h>
#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/ptr.h>

namespace nx::sdk::analytics {

class TimestampedObjectMetadata: public RefCountable<ITimestampedObjectMetadata>
{
public:
    TimestampedObjectMetadata();

    virtual const char* subtype() const override;
    virtual int attributeCount() const override;
    virtual int64_t timestampUs() const override;
    virtual const char* typeId() const override;
    virtual float confidence() const override;

    void setTypeId(std::string typeId);
    void setConfidence(float confidence);
    void setTrackId(const Uuid& value);
    void setSubtype(const std::string& value);
    void addAttribute(nx::sdk::Ptr<Attribute> attribute);
    void addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value);
    void setBoundingBox(const Rect& rect);
    void setTimestampUs(int64_t timestampUs);

protected:
    virtual const IAttribute* getAttribute(int index) const override;
    virtual void getTrackId(Uuid* outValue) const override;
    virtual void getBoundingBox(Rect* outValue) const override;

private:
    const nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> m_objectMetadata;
    int64_t m_timestampUs = 0;
};

} // namespace nx::sdk::analytics
