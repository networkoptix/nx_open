#pragma once

#include <nx/sdk/i_plugin_event.h>
#include <nx/sdk/i_string.h>
#include <plugins/plugin_tools.h>

namespace nx {
namespace sdk {
namespace common {

class PluginEvent: public nxpt::CommonRefCounter<IPluginEvent>
{
public:
    PluginEvent(IPluginEvent::Level level, std::string caption, std::string description);

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual Level level() const override;
    virtual const char* caption() const override;
    virtual const char* description() const override;

    void setLevel(IPluginEvent::Level level);
    void setCaption(std::string caption);
    void setDescription(std::string description);

private:
    IPluginEvent::Level m_level = IPluginEvent::Level::info;
    std::string m_caption;
    std::string m_description;
};

} // namespace common
} // namespace sdk
} // namespace nx
