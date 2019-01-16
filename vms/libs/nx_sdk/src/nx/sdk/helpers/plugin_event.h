#pragma once

#include <string>

#include <plugins/plugin_tools.h>
#include <nx/sdk/i_plugin_event.h>

namespace nx {
namespace sdk {

class PluginEvent: public nxpt::CommonRefCounter<IPluginEvent>
{
public:
    PluginEvent(Level level, std::string caption, std::string description);

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual Level level() const override;
    virtual const char* caption() const override;
    virtual const char* description() const override;

    void setLevel(IPluginEvent::Level level);
    void setCaption(std::string caption);
    void setDescription(std::string description);

private:
    Level m_level = Level::info;
    std::string m_caption;
    std::string m_description;
};

} // namespace sdk
} // namespace nx
