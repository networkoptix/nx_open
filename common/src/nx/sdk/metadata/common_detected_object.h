#pragma once

#include <string>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_event_metadata_packet.h>
#include "abstract_detection_metadata_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

class CommonDetectedObject: public nxpt::CommonRefCounter<AbstarctDetectedObject>
{
public:
    virtual ~CommonDetectedObject();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nxpl::NX_GUID eventTypeId() const override;
    virtual float confidence() const override;
    virtual nxpl::NX_GUID id() const override;
    virtual NX_ASCII const char* objectSubType() const override;
    virtual NX_LOCALE_DEPENDENT const Attribute* attribute(int index) const override;
    virtual int attributeCount() const override;
    virtual const char* auxilaryData() const override;
    virtual Rect boundingBox() const override;

    void setEventTypeId(const nxpl::NX_GUID& eventTypeId);
    void setConfidence(float confidence);
    void setId(const nxpl::NX_GUID& value);
    void setObjectSubType(const std::string& value);
    void seAttributes(const std::vector<NX_LOCALE_DEPENDENT Attribute>& value);
    void setAuxilaryData(const std::string& value);
    void setBoundingBox(const Rect& rect);

private:
    nxpl::NX_GUID m_eventTypeId;
    float m_confidence = 1.0;

    nxpl::NX_GUID m_id;
    std::string m_objectSubType;
    std::vector<NX_LOCALE_DEPENDENT Attribute> m_attributes;
    std::string m_auxilaryData;
    Rect m_rect;
};


} // namespace nx
} // namespace sdk
} // namespace nx