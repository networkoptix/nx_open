#include "util.h"

#include <transaction/transaction.h>

namespace nx::test {

static bool compare(
    const ::ec2::QnAbstractTransaction::PersistentInfo& vmsTranPersistentInfo,
    const nx::clusterdb::engine::PersistentInfo& cloudCommandPersistentInfo)
{
    return vmsTranPersistentInfo.dbID == cloudCommandPersistentInfo.dbID
        && vmsTranPersistentInfo.sequence == cloudCommandPersistentInfo.sequence
        && vmsTranPersistentInfo.timestamp == cloudCommandPersistentInfo.timestamp;
}

bool compare(
    const ::ec2::QnAbstractTransaction& vmsTran,
    const nx::clusterdb::engine::CommandHeader& cloudCommandHeader)
{
    return vmsTran.command == cloudCommandHeader.command
        && vmsTran.historyAttributes.author == cloudCommandHeader.historyAttributes.author
        && vmsTran.peerID == cloudCommandHeader.peerID
        && compare(vmsTran.persistentInfo, cloudCommandHeader.persistentInfo)
        && vmsTran.transactionType == cloudCommandHeader.transactionType;
}

bool compare(
    const ::ec2::ApiTransactionData& vmsTran,
    const nx::clusterdb::engine::CommandData& cloudCommand)
{
    return vmsTran.dataSize == cloudCommand.dataSize
        && compare(vmsTran.tran, cloudCommand.tran)
        && vmsTran.tranGuid == cloudCommand.tranGuid;
}

bool compare(
    ::ec2::ApiTransactionDataList vmsTransactionLog,
    nx::clusterdb::engine::CommandDataList cloudCommandLog)
{
    std::sort(
        vmsTransactionLog.begin(), vmsTransactionLog.end(),
        [](const auto& left, const auto& right) { return left.tranGuid < right.tranGuid; });
    
    std::sort(
        cloudCommandLog.begin(), cloudCommandLog.end(),
        [](const auto& left, const auto& right) { return left.tranGuid < right.tranGuid; });

    if (vmsTransactionLog.size() != cloudCommandLog.size())
        return false;

    for (std::size_t i = 0; i < vmsTransactionLog.size(); ++i)
    {
        if (!compare(vmsTransactionLog[i], cloudCommandLog[i]))
            return false;
    }

    return true;
}

} // namespace nx::test
