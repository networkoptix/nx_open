#pragma once

#include <string>
#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/uuid.h>
#include <nx/sdk/analytics/i_object.h>
#include <nx/sdk/analytics/helpers/attribute.h>

namespace nx {
namespace sdk {
namespace analytics {

class Object: public nxpt::CommonRefCounter<IObject>
{
public:
    virtual ~Object();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* typeId() const override;
    virtual float confidence() const override;
    virtual Uuid id() const override;
    virtual const char* objectSubType() const override;
    virtual const IAttribute* attribute(int index) const override;
    virtual int attributeCount() const override;
    virtual const char* auxiliaryData() const override;
    virtual Rect boundingBox() const override;

    void setTypeId(std::string typeId);
    void setConfidence(float confidence);
    void setId(const Uuid& value);
    void setObjectSubType(const std::string& value);
    void setAttributes(const std::vector<Attribute>& value);
    void setAuxiliaryData(std::string value);
    void setBoundingBox(const Rect& rect);

private:
    std::string m_typeId;

    float m_confidence = 1.0;

    Uuid m_id;
    std::string m_objectSubType;
    std::vector<Attribute> m_attributes;
    std::string m_auxiliaryData;
    Rect m_rect;
};

} // namespace nx
} // namespace sdk
} // namespace nx
