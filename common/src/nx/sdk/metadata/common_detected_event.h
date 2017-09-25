#pragma once

#include <string>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_event_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

class CommonDetectedEvent: public nxpt::CommonRefCounter<AbstractDetectedEvent>
{
public:
    virtual ~CommonDetectedEvent();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nxpl::NX_GUID eventTypeId() const override;

    virtual float confidence() const override;

    virtual const char* caption() const override;

    virtual const char* description() const override;

    virtual const char* auxilaryData() const override;

    virtual bool isActive() const override;

    virtual void setEventTypeId(const nxpl::NX_GUID& eventTypeId);

    virtual void setConfidence(float confidence);

    virtual void setCaption(const std::string& caption);

    virtual void setDescription(const std::string& description);

    virtual void setAuxilaryData(const std::string& auxilaryData);

    virtual void setIsActive(bool isActive);

private:
    nxpl::NX_GUID m_eventTypeId;
    float m_confidence = 1.0;
    std::string m_caption;
    std::string m_description;
    std::string m_auxilaryData;
    bool m_isActive = false;
};


} // namespace nx
} // namespace sdk
} // namespace nx