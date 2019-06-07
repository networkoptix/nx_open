#pragma once

#include <string>
#include <vector>

#include <nx/sdk/analytics/i_event_metadata.h>

#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/attribute.h>

namespace nx {
namespace sdk {
namespace analytics {

class EventMetadata: public RefCountable<IEventMetadata>
{
public:
    virtual const char* typeId() const override;
    virtual float confidence() const override;
    virtual const IAttribute* attribute(int index) const override;
    virtual int attributeCount() const override;
    virtual const char* caption() const override;
    virtual const char* description() const override;
    virtual bool isActive() const override;

    void setTypeId(std::string typeId);
    void setConfidence(float confidence);
    void setCaption(const std::string& caption);
    void setDescription(const std::string& description);
    void setIsActive(bool isActive);
    void addAttribute(nx::sdk::Ptr<Attribute> attribute);
    void addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value);

private:
    std::string m_typeId;
    float m_confidence = 1.0;
    std::string m_caption;
    std::string m_description;
    bool m_isActive = false;
    std::vector<nx::sdk::Ptr<Attribute>> m_attributes;
};

} // namespace nx
} // namespace sdk
} // namespace nx
