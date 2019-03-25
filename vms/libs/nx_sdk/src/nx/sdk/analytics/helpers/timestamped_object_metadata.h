#pragma once

#include <vector>

#include <nx/sdk/analytics/i_timestamped_object_metadata.h>

#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/attribute.h>

namespace nx {
namespace sdk {
namespace analytics {

class TimestampedObjectMetadata: public RefCountable<ITimestampedObjectMetadata>
{
public:
    TimestampedObjectMetadata();

    virtual Uuid id() const override;

    virtual const char* subtype() const override;

    virtual const IAttribute* attribute(int index) const override;

    virtual int attributeCount() const override;

    virtual const char* auxiliaryData() const override;

    virtual Rect boundingBox() const override;

    virtual int64_t timestampUs() const override;

    virtual const char* typeId() const override;

    virtual float confidence() const override;

    void setTypeId(std::string typeId);
    void setConfidence(float confidence);
    void setId(const Uuid& value);
    void setSubtype(const std::string& value);
    void addAttribute(Attribute attribute);
    void addAttributes(const std::vector<Attribute>& value);
    void setAuxiliaryData(std::string value);
    void setBoundingBox(const Rect& rect);
    void setTimestampUs(int64_t timestampUs);

private:
    nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> m_objectMetadata;
    int64_t m_timestampUs = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
