#pragma once

#include <string>
#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/objects_metadata_packet.h>
#include <nx/sdk/metadata/common_attribute.h>

namespace nx {
namespace sdk {
namespace metadata {

class CommonObject: public nxpt::CommonRefCounter<Object>
{
public:
    virtual ~CommonObject();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* typeId() const override;
    virtual float confidence() const override;
    virtual nxpl::NX_GUID id() const override;
    virtual NX_ASCII const char* objectSubType() const override;
    virtual NX_LOCALE_DEPENDENT const Attribute* attribute(int index) const override;
    virtual int attributeCount() const override;
    virtual const char* auxilaryData() const override;
    virtual Rect boundingBox() const override;

    void setTypeId(std::string typeId);
    void setConfidence(float confidence);
    void setId(const nxpl::NX_GUID& value);
    void setObjectSubType(const std::string& value);
    void setAttributes(const std::vector<CommonAttribute>& value);
    void setAuxilaryData(std::string value);
    void setBoundingBox(const Rect& rect);

private:
    std::string m_typeId;

    float m_confidence = 1.0;

    nxpl::NX_GUID m_id{{0}};
    std::string m_objectSubType;
    std::vector<CommonAttribute> m_attributes;
    std::string m_auxilaryData;
    Rect m_rect;
};

} // namespace nx
} // namespace sdk
} // namespace nx