#include "outgoing_command_filter.h"

#include "transaction_log.h"

namespace nx::data_sync_engine {

static const QnUuid kCdbGuid("{674bafd7-4eec-4bba-84aa-a1baea7fc6db}");

OutgoingCommandFilter::OutgoingCommandFilter()
{
}

void OutgoingCommandFilter::configure(
    const OutgoingCommandFilterConfiguration& configuration)
{
    m_configuration = configuration;
}

bool OutgoingCommandFilter::satisfies(const CommandHeader& commandHeader) const
{
    if (m_configuration.sendOnlyOwnCommands && commandHeader.peerID != kCdbGuid)
        return false;

    return true;
}

void OutgoingCommandFilter::updateReadFilter(ReadCommandsFilter* readFilter) const
{
    if (m_configuration.sendOnlyOwnCommands)
        readFilter->sources = {kCdbGuid};
}

} // namespace nx::data_sync_engine
