#include "plugin.h"

#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>

#include "engine.h"

namespace nx {
namespace sdk {
namespace analytics {

Plugin::Plugin(
    std::string pluginName,
    std::string pluginManifest,
    CreateEngine createEngine)
    :
    m_name(std::move(pluginName)),
    m_jsonManifest(std::move(pluginManifest)),
    m_createEngine(std::move(createEngine))
{
    NX_PRINT << "Created " << m_name << "[" << this << "]";
}

Plugin::~Plugin()
{
    NX_PRINT << "Destroyed " << m_name << "[" << this << "]";
}

const char* Plugin::name() const
{
    return m_name.c_str();
}

void Plugin::setUtilityProvider(IUtilityProvider* utilityProvider)
{
    utilityProvider->addRef();
    m_utilityProvider.reset(utilityProvider);
}

const IString* Plugin::manifest(nx::sdk::Error* outError) const
{
    if (outError)
        *outError = nx::sdk::Error::noError;

    return new String(m_jsonManifest);
}

IEngine* Plugin::createEngine(Error* outError)
{
    IEngine* engine = m_createEngine(this);
    if (!engine)
    {
        NX_PRINT << "ERROR: " << m_name << ": createEngine() failed";
        *outError = Error::unknownError;
        return nullptr;
    }
    return engine;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
