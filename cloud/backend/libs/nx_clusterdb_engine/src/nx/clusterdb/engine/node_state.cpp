#include "node_state.h"

#include <algorithm>

namespace nx::clusterdb::engine {

bool NodeState::containsDataMissingIn(const NodeState& other) const
{
    auto thisIter = nodeSequence.cbegin();
    auto rightIter = other.nodeSequence.cbegin();

    for (;;)
    {
        if (thisIter == nodeSequence.cend())
            return false;
        if (rightIter == other.nodeSequence.cend())
            return true;

        if (thisIter->first < rightIter->first)
            return true;

        if (rightIter->first < thisIter->first)
        {
            ++rightIter;
            continue;
        }

        if (thisIter->second > rightIter->second)
            return true;

        ++thisIter;
        ++rightIter;
    }
}

bool NodeState::operator<(const NodeState& right) const
{
    return std::lexicographical_compare(
        nodeSequence.begin(), nodeSequence.end(),
        right.nodeSequence.begin(), right.nodeSequence.end());
}

long long NodeState::sequence(const NodeStateKey& node, long long defaultValue) const
{
    auto it = nodeSequence.find(node);
    return it != nodeSequence.end() ? it->second : defaultValue;
}

std::string NodeState::toString() const
{
    std::string str;
    for (auto it = nodeSequence.begin(); it != nodeSequence.end(); ++it)
    {
        if (!str.empty())
            str += ", ";

        str += "{" + it->first.nodeId.toSimpleString().toStdString() + ", " +
            it->first.dbId.toSimpleString().toStdString() + "}";
        str += ": ";
        str += std::to_string(it->second);
    }

    return str;
}

} // namespace nx::clusterdb::engine
