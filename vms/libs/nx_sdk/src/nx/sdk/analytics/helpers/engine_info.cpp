#include <nx/sdk/analytics/helpers/engine_info.h>

namespace nx {
namespace sdk {
namespace analytics {

const char* EngineInfo::id() const
{
    return m_name.c_str();
}

const char* EngineInfo::name() const
{
    return m_name.c_str();
}

void EngineInfo::setId(std::string id)
{
    m_id = std::move(id);
}

void EngineInfo::setName(std::string name)
{
    m_name = std::move(name);
}

} // namespace analytics
} // namespace sdk
} // namespace nx
