#include "command.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log_message.h>

namespace nx::clusterdb::engine {

bool HistoryAttributes::operator==(const HistoryAttributes& rhs) const
{
    return author == rhs.author;
}

//-------------------------------------------------------------------------------------------------

bool PersistentInfo::isNull() const
{
    return dbID.isNull();
}

bool PersistentInfo::operator==(const PersistentInfo& other) const
{
    return dbID == other.dbID
        && sequence == other.sequence
        && timestamp == other.timestamp;
}

uint qHash(const nx::clusterdb::engine::PersistentInfo& id)
{
    return ::qHash(QByteArray(id.dbID.toRfc4122())
        .append((const char*)& id.timestamp, sizeof(id.timestamp)), id.sequence);
}

//-------------------------------------------------------------------------------------------------

CommandHeader::CommandHeader(int value, QnUuid peerId):
    command(value),
    peerID(std::move(peerId))
{
}

std::string CommandHeader::toString() const
{
    return lm("command=%1 time=%2 seq=%3 peer=%4 dbId=%5")
        .args(command,
            persistentInfo.timestamp,
            persistentInfo.sequence,
            peerID.toString(),
            persistentInfo.dbID.toString()).toStdString();
}

bool CommandHeader::operator==(const CommandHeader& rhs) const
{
    return command == rhs.command
        && peerID == rhs.peerID
        && persistentInfo == rhs.persistentInfo
        && transactionType == rhs.transactionType
        && historyAttributes == rhs.historyAttributes;
}

bool CommandHeader::operator!=(const CommandHeader& rhs) const
{
    return !(*this == rhs);
}

//-------------------------------------------------------------------------------------------------

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (HistoryAttributes)(PersistentInfo)(CommandHeader),
    (json)(ubjson),
    _Fields,
    (optional, true))

} // namespace nx::clusterdb::engine
