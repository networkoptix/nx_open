#pragma once

#include <map>

#include <nx/utils/uuid.h>

#include "command.h"

namespace nx::clusterdb::engine {

struct NodeStateKey
{
    // TODO: #ak Refactor this to std::string.
    QnUuid nodeId;
    QnUuid dbId;

    bool operator<(const NodeStateKey& right) const
    {
        return
            nodeId < right.nodeId ? true :
            nodeId > right.nodeId ? false :
            dbId < right.dbId;
    }

    static NodeStateKey build(const CommandHeader& commandHeader)
    {
        return NodeStateKey{
            commandHeader.peerID,
            commandHeader.persistentInfo.dbID};
    }
};

class NX_DATA_SYNC_ENGINE_API NodeState
{
public:
    std::map<NodeStateKey, long long> nodeSequence;

    /**
     * @return true If this has a greater sequence than other for some node id.
     */
    bool containsDataMissingIn(const NodeState& other) const;

    /**
     * Performs lexicographical comparison.
     */
    bool operator<(const NodeState& right) const;

    /**
     * @return defaultValue if node is unknown.
     */
    long long sequence(const NodeStateKey& node, long long defaultValue = 0) const;

    std::string toString() const;
};

} // namespace nx::clusterdb::engine
