// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/sdk/analytics/i_engine_info.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx::sdk::analytics {

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

} // namespace nx::sdk::analytics
