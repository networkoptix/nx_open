// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/sdk/analytics/i_event_metadata.h>
#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/ptr.h>

namespace nx::sdk::analytics {

class EventMetadata: public RefCountable<IEventMetadata>
{
public:
    virtual const char* typeId() const override;
    virtual float confidence() const override;
    virtual int attributeCount() const override;
    virtual const char* caption() const override;
    virtual const char* description() const override;
    virtual bool isActive() const override;
    virtual const char* key() const override;

    void setTypeId(std::string typeId);
    void setConfidence(float confidence);
    void setCaption(const std::string& caption);
    void setDescription(const std::string& description);
    void setIsActive(bool isActive);
    void addAttribute(nx::sdk::Ptr<Attribute> attribute);
    void addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value);
    void setTrackId(Uuid trackId);
    void setKey(std::string key);

protected:
    virtual const IAttribute* getAttribute(int index) const override;
    virtual void getTrackId(Uuid* outValue) const override;

private:
    std::string m_typeId;
    float m_confidence = 1.0;
    std::string m_caption;
    std::string m_description;
    bool m_isActive = false;
    std::vector<nx::sdk::Ptr<Attribute>> m_attributes;
    Uuid m_trackId;
    std::string m_key;
};

} // namespace nx::sdk::analytics
