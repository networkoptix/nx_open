#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>

#include "command.h"

namespace nx::clusterdb::engine {

struct NX_DATA_SYNC_ENGINE_API CommandData
{
    QnUuid tranGuid;
    CommandHeader tran;
    int dataSize = 0;

    CommandData() = default;

    CommandData(const QnUuid& peerGuid)
    {
        tran.peerID = peerGuid;
    }

    CommandData(const CommandData&) = default;
    CommandData& operator=(const CommandData&) = default;
    CommandData(CommandData&&) = default;
    CommandData& operator=(CommandData&&) = default;

    bool operator==(const CommandData& right) const
    {
        return tranGuid == right.tranGuid
            && tran == right.tran
            && dataSize == right.dataSize;
    }
};

#define CommandData_Fields (tranGuid)(tran)(dataSize)

QN_FUSION_DECLARE_FUNCTIONS(
    CommandData,
    (json)(ubjson),
    NX_DATA_SYNC_ENGINE_API)

using CommandDataList = std::vector<CommandData>;

} // namespace nx::clusterdb::engine
