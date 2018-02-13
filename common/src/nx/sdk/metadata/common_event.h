#pragma once

#include <string>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/events_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

class CommonEvent: public nxpt::CommonRefCounter<Event>
{
public:
    virtual ~CommonEvent();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nxpl::NX_GUID typeId() const override;

    virtual float confidence() const override;

    virtual const char* caption() const override;

    virtual const char* description() const override;

    virtual const char* auxilaryData() const override;

    virtual bool isActive() const override;

    virtual void setTypeId(const nxpl::NX_GUID& typeId);

    virtual void setConfidence(float confidence);

    virtual void setCaption(const std::string& caption);

    virtual void setDescription(const std::string& description);

    virtual void setAuxilaryData(const std::string& auxilaryData);

    virtual void setIsActive(bool isActive);

private:
    nxpl::NX_GUID m_typeId;
    float m_confidence = 1.0;
    std::string m_caption;
    std::string m_description;
    std::string m_auxilaryData;
    bool m_isActive = false;
};


} // namespace nx
} // namespace sdk
} // namespace nx