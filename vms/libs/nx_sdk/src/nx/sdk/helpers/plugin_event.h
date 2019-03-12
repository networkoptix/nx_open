#pragma once

#include <string>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_plugin_event.h>

namespace nx {
namespace sdk {

class PluginEvent: public RefCountable<IPluginEvent>
{
public:
    PluginEvent(Level level, std::string caption, std::string description);

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
