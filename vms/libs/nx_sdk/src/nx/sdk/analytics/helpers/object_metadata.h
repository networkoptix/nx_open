#pragma once

#include <string>
#include <vector>

#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/ptr.h>

#include <nx/sdk/uuid.h>
#include <nx/sdk/analytics/i_object_metadata.h>
#include <nx/sdk/helpers/attribute.h>

namespace nx {
namespace sdk {
namespace analytics {

class ObjectMetadata: public RefCountable<IObjectMetadata>
{
public:
    virtual const char* typeId() const override;
    virtual float confidence() const override;
    virtual Uuid id() const override;
    virtual const char* subtype() const override;
    virtual const IAttribute* attribute(int index) const override;
    virtual int attributeCount() const override;
    virtual Rect boundingBox() const override;

    void setTypeId(std::string typeId);
    void setConfidence(float confidence);
    void setId(const Uuid& value);
    void setSubtype(const std::string& value);
    void addAttribute(nx::sdk::Ptr<Attribute> attribute);
    void addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value);
    void setBoundingBox(const Rect& rect);

private:
    std::string m_typeId;
    float m_confidence = 1.0;
    Uuid m_id;
    std::string m_subtype;
    std::vector<nx::sdk::Ptr<Attribute>> m_attributes;
    Rect m_rect;
};

} // namespace nx
} // namespace sdk
} // namespace nx
