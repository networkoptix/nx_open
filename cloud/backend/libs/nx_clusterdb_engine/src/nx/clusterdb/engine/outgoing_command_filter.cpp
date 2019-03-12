#include "outgoing_command_filter.h"

#include "transaction_log.h"

namespace nx::clusterdb::engine {

OutgoingCommandFilter::OutgoingCommandFilter(const QnUuid& selfPeerId):
    m_selfPeerId(selfPeerId)
{
}

void OutgoingCommandFilter::configure(
    const OutgoingCommandFilterConfiguration& configuration)
{
    m_configuration = configuration;
}

bool OutgoingCommandFilter::satisfies(const CommandHeader& commandHeader) const
{
    if (m_configuration.sendOnlyOwnCommands && commandHeader.peerID != m_selfPeerId)
        return false;

    return true;
}

vms::api::TranState OutgoingCommandFilter::filter(const vms::api::TranState& value) const
{
    vms::api::TranState result;
    for (auto it = value.values.begin(); it != value.values.end(); ++it)
    {
        if (m_configuration.sendOnlyOwnCommands && it.key().id != m_selfPeerId)
            continue;

        result.values.insert(it.key(), it.value());
    }

    return result;
}

void OutgoingCommandFilter::updateReadFilter(ReadCommandsFilter* readFilter) const
{
    if (m_configuration.sendOnlyOwnCommands)
        readFilter->sources = {m_selfPeerId};
}

} // namespace nx::clusterdb::engine
