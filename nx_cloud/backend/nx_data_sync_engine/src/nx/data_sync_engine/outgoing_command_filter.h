#pragma once

#include "command.h"

namespace nx::data_sync_engine {

struct ReadCommandsFilter;

struct OutgoingCommandFilterConfiguration
{
    bool sendOnlyOwnCommands = false;
};

//-------------------------------------------------------------------------------------------------

class OutgoingCommandFilter
{
public:
    OutgoingCommandFilter();

    void configure(const OutgoingCommandFilterConfiguration& configuration);

    bool satisfies(const CommandHeader& commandHeader) const;

    void updateReadFilter(ReadCommandsFilter* readFilter) const;

private:
    OutgoingCommandFilterConfiguration m_configuration;
};

} // namespace nx::data_sync_engine
