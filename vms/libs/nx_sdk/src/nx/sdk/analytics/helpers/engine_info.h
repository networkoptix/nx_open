#pragma once

#include <string>

#include <nx/sdk/analytics/i_engine_info.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx {
namespace sdk {
namespace analytics {

class EngineInfo: public RefCountable<IEngineInfo>
{
public:
    virtual const char* id() const override;

    virtual const char* name() const override;

    void setId(std::string id);

    void setName(std::string name);

private:
    std::string m_id;
    std::string m_name;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
