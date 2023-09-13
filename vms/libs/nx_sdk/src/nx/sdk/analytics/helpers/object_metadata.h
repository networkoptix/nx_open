// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/sdk/analytics/i_object_metadata.h>
#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/uuid.h>

namespace nx::sdk::analytics {

class ObjectMetadata: public RefCountable<IObjectMetadata>
{
public:
    virtual const char* typeId() const override;
    virtual float confidence() const override;
    virtual const char* subtype() const override;
    virtual int attributeCount() const override;

    void setTypeId(std::string typeId);
    void setConfidence(float confidence);
    void setTrackId(const Uuid& value);
    void setSubtype(const std::string& value);
    void addAttribute(nx::sdk::Ptr<Attribute> attribute);
    void addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value);
    void addAttributes(std::vector<nx::sdk::Ptr<Attribute>>&& value);
    void setBoundingBox(const Rect& rect);

protected:
    virtual const IAttribute* getAttribute(int index) const override;
    virtual void getTrackId(Uuid* outValue) const override;
    virtual void getBoundingBox(Rect* outValue) const override;

private:
    std::string m_typeId;
    float m_confidence = 1.0;
    Uuid m_trackId;
    std::string m_subtype;
    std::vector<nx::sdk::Ptr<Attribute>> m_attributes;
    Rect m_rect;
};

} // namespace nx::sdk::analytics
