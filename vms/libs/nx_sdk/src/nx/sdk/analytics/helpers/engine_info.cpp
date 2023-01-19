// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/sdk/analytics/helpers/engine_info.h>

namespace nx::sdk::analytics {

const char* EngineInfo::id() const
{
    return m_id.c_str();
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

} // namespace nx::sdk::analytics
