#pragma once

#include <string>
#include <vector>

#include <nx/sdk/helpers/ref_countable.h>

#include <nx/sdk/uuid.h>
#include <nx/sdk/analytics/i_object_metadata.h>
#include <nx/sdk/analytics/helpers/attribute.h>

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
    virtual const char* auxiliaryData() const override;
    virtual Rect boundingBox() const override;

    void setTypeId(std::string typeId);
    void setConfidence(float confidence);
    void setId(const Uuid& value);
    void setSubtype(const std::string& value);
    void addAttribute(Attribute attribute);
    void addAttributes(const std::vector<Attribute>& value);
    void setAuxiliaryData(std::string value);
    void setBoundingBox(const Rect& rect);

private:
    std::string m_typeId;
    float m_confidence = 1.0;
    Uuid m_id;
    std::string m_subtype;
    std::vector<Attribute> m_attributes;
    std::string m_auxiliaryData;
    Rect m_rect;
};

} // namespace nx
} // namespace sdk
} // namespace nx
