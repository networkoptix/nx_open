////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "node.h"


namespace stree
{
    ////////////////////////////////////////////////////////////
    //// class SequenceNode
    ////////////////////////////////////////////////////////////
    //!Implementation of AbstractNode::get
    void SequenceNode::get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const
    {
        for( auto it = m_children.begin();
            it != m_children.end();
            ++it )
        {
            it->second->get( in, out );
        }
    }
    
    //!Implementation of AbstractNode::addChild
    bool SequenceNode::addChild( const QVariant& value, std::unique_ptr<AbstractNode> child )
    {
        m_children.emplace( value.value<int>(), std::move(child) );
        return true;
    }


    ////////////////////////////////////////////////////////////
    //// class ResPresenceNode
    ////////////////////////////////////////////////////////////
    ResPresenceNode::ResPresenceNode(int matchResID)
    :
        m_matchResID(matchResID)
    {
    }

    void ResPresenceNode::get(const AbstractResourceReader& in, AbstractResourceWriter* const out) const
    {
        NX_LOG(lit("Stree. Condition. Selecting child by resource %1").arg(m_matchResID), cl_logDEBUG2);

        QVariant value;
        if (!in.get(m_matchResID, &value))
        {
            NX_LOG(lit("Stree. Presence Condition. Resource (%1) not found in input data").arg(m_matchResID), cl_logDEBUG2);
            return;
        }

        const int intVal = value.toBool() ? 1 : 0;
        if (!m_children[intVal])
        {
            NX_LOG(lit("Stree. Presence Condition. Could not find child by value %1").arg(value.toString()), cl_logDEBUG2);
            return;
        }

        NX_LOG(lit("Stree. Presence Condition. Found child by search value %1").arg(value.toString()), cl_logDEBUG2);
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


    ////////////////////////////////////////////////////////////
    //// class SetNode
    ////////////////////////////////////////////////////////////
    SetNode::SetNode(
        int resourceID,
        const QVariant& valueToSet )
    :
        m_resourceID( resourceID ),
        m_valueToSet( valueToSet )
    {
    }

    //!Implementation of AbstractNode::get
    void SetNode::get( const AbstractResourceReader& /*in*/, AbstractResourceWriter* const out ) const
    {
        out->put( m_resourceID, m_valueToSet );
    }
    
    //!Implementation of AbstractNode::addChild
    bool SetNode::addChild( const QVariant& /*value*/, std::unique_ptr<AbstractNode> child )
    {
        if( m_child )
            return false;
        m_child = std::move(child);
        return true;
    }

}
