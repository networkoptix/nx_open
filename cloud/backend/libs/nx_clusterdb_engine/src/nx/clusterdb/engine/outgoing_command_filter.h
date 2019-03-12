#pragma once

#include "command.h"

namespace nx::clusterdb::engine {

struct ReadCommandsFilter;

struct OutgoingCommandFilterConfiguration
{
    bool sendOnlyOwnCommands = false;
};

//-------------------------------------------------------------------------------------------------

class OutgoingCommandFilter
{
public:
    OutgoingCommandFilter(const QnUuid& selfPeerId);

    void configure(const OutgoingCommandFilterConfiguration& configuration);

    bool satisfies(const CommandHeader& commandHeader) const;

    vms::api::TranState filter(const vms::api::TranState& value) const;

    void updateReadFilter(ReadCommandsFilter* readFilter) const;

private:
    OutgoingCommandFilterConfiguration m_configuration;
    const QnUuid m_selfPeerId;
};

} // namespace nx::clusterdb::engine
