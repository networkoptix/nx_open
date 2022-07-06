// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "node.h"

#include <nx/utils/string.h>

namespace nx::utils::stree {

//-------------------------------------------------------------------------------------------------
// class SequenceNode.

void SequenceNode::get(const AbstractAttributeReader& in, AbstractAttributeWriter* const out) const
{
    for (auto it = m_children.begin(); it != m_children.end(); ++it)
        it->second->get(in, out);
}

bool SequenceNode::addChild(const std::string_view& value, std::unique_ptr<AbstractNode> child)
{
    m_children.emplace(nx::utils::stoi(value), std::move(child));
    return true;
}


//-------------------------------------------------------------------------------------------------
// class AttrPresenceNode.

AttrPresenceNode::AttrPresenceNode(std::string attrToMatchName):
    m_attrToMatchName(std::move(attrToMatchName))
{
}

void AttrPresenceNode::get(const AbstractAttributeReader& in, AbstractAttributeWriter* const out) const
{
    NX_TRACE(this, "Condition. Selecting child by attribute %1", m_attrToMatchName);

    std::string value;
    const int intVal = in.contains(m_attrToMatchName) ? 1 : 0;
    if (!m_children[intVal])
    {
        NX_TRACE(this, "Presence Condition. Could not find child by value %1", intVal);
        return;
    }

    NX_TRACE(this, "Presence Condition. Found child by search value %1", intVal);
    m_children[intVal]->get(in, out);
}

bool AttrPresenceNode::addChild(const std::string_view& str, std::unique_ptr<AbstractNode> child)
{
    int intVal = -1;

    if (stricmp(str, "true") == 0 || str == "1")
        intVal = 1;
    else if (stricmp(str, "false") == 0 || str == "0")
        intVal = 0;
    else
        return false;

    if (m_children[intVal])
        return false;

    m_children[intVal] = std::move(child);
    return true;
}


//-------------------------------------------------------------------------------------------------
// class SetNode.

SetNode::SetNode(std::string name, std::string value):
    m_name(std::move(name)),
    m_value(std::move(value))
{
}

void SetNode::get(const AbstractAttributeReader& /*in*/, AbstractAttributeWriter* const out) const
{
    NX_TRACE(this, "SetNode. Add (%1; %2)", m_name, m_value);
    out->put(m_name, m_value);
}

bool SetNode::addChild(const std::string_view& /*value*/, std::unique_ptr<AbstractNode> child)
{
    if (m_child)
        return false;

    m_child = std::move(child);
    return true;
}

} // namespace nx::utils::stree
