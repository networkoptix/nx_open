#include "node.h"

namespace nx {
namespace utils {
namespace stree {

//-------------------------------------------------------------------------------------------------
// class SequenceNode.

void SequenceNode::get(const AbstractResourceReader& in, AbstractResourceWriter* const out) const
{
    for (auto it = m_children.begin();
        it != m_children.end();
        ++it)
    {
        it->second->get(in, out);
    }
}

bool SequenceNode::addChild(const QVariant& value, std::unique_ptr<AbstractNode> child)
{
    m_children.emplace(value.value<int>(), std::move(child));
    return true;
}


//-------------------------------------------------------------------------------------------------
// class ResPresenceNode.

ResPresenceNode::ResPresenceNode(int matchResId):
    m_matchResId(matchResId)
{
}

void ResPresenceNode::get(const AbstractResourceReader& in, AbstractResourceWriter* const out) const
{
#ifdef NX_STREE_ENABLE_DEBUG_LOGGING
    NX_LOG(lit("Stree. Condition. Selecting child by resource %1").arg(m_matchResId), cl_logDEBUG2);
#endif

    QVariant value;
    const bool resPresentInInputData = in.get(m_matchResId, &value);
    const int intVal = resPresentInInputData ? 1 : 0;
    if (!m_children[intVal])
    {
#ifdef NX_STREE_ENABLE_DEBUG_LOGGING
        NX_LOG(lit("Stree. Presence Condition. Could not find child by value %1").arg(intVal), cl_logDEBUG2);
#endif
        return;
    }

#ifdef NX_STREE_ENABLE_DEBUG_LOGGING
    NX_LOG(lit("Stree. Presence Condition. Found child by search value %1").arg(intVal), cl_logDEBUG2);
#endif
    m_children[intVal]->get(in, out);
}

bool ResPresenceNode::addChild(const QVariant& value, std::unique_ptr<AbstractNode> child)
{
    const int intVal = value.toBool() ? 1 : 0;
    if (m_children[intVal])
        return false;
    m_children[intVal] = std::move(child);
    return true;
}


//-------------------------------------------------------------------------------------------------
// class SetNode.

SetNode::SetNode(
    int resourceID,
    const QVariant& valueToSet)
    :
    m_resourceID(resourceID),
    m_valueToSet(valueToSet)
{
}

void SetNode::get(const AbstractResourceReader& /*in*/, AbstractResourceWriter* const out) const
{
    out->put(m_resourceID, m_valueToSet);
}

bool SetNode::addChild(const QVariant& /*value*/, std::unique_ptr<AbstractNode> child)
{
    if (m_child)
        return false;
    m_child = std::move(child);
    return true;
}

} // namespace stree
} // namespace utils
} // namespace nx
