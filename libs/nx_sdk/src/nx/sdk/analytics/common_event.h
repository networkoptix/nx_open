#pragma once

#include <string>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/events_metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

class NX_SDK_API CommonEvent: public nxpt::CommonRefCounter<Event>
{
public:
    virtual ~CommonEvent();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual const char* typeId() const override;
    virtual float confidence() const override;
    virtual const char* caption() const override;
    virtual const char* description() const override;
    virtual const char* auxiliaryData() const override;
    virtual bool isActive() const override;
    virtual void setTypeId(std::string typeId);
    virtual void setConfidence(float confidence);
    virtual void setCaption(const std::string& caption);
    virtual void setDescription(const std::string& description);
    virtual void setAuxiliaryData(const std::string& auxiliaryData);
    virtual void setIsActive(bool isActive);

private:
    std::string m_typeId;
    float m_confidence = 1.0;
    std::string m_caption;
    std::string m_description;
    std::string m_auxiliaryData;
    bool m_isActive = false;
};

} // namespace nx
} // namespace sdk
} // namespace nx