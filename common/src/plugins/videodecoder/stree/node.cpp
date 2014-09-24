////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "node.h"


namespace stree
{
    ////////////////////////////////////////////////////////////
    //// class SequenceNode
    ////////////////////////////////////////////////////////////
    SequenceNode::~SequenceNode()
    {
        for( std::multimap<int, AbstractNode*>::iterator
            it = m_children.begin();
            it != m_children.end();
             )
        {
            delete it->second;
            m_children.erase( it++ );
        }
    }

    //!Implementation of AbstractNode::get
    void SequenceNode::get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const
    {
        for( std::multimap<int, AbstractNode*>::const_iterator
            it = m_children.begin();
            it != m_children.end();
            ++it )
        {
            it->second->get( in, out );
        }
    }
    
    //!Implementation of AbstractNode::addChild
    bool SequenceNode::addChild( const QVariant& value, AbstractNode* child )
    {
        m_children.insert( std::make_pair( value.value<int>(), child ) );
        return true;
    }


    ////////////////////////////////////////////////////////////
    //// class SetNode
    ////////////////////////////////////////////////////////////
    SetNode::SetNode(
        int resourceID,
        const QVariant& valueToSet )
    :
        m_child( NULL ),
        m_resourceID( resourceID ),
        m_valueToSet( valueToSet )
    {
    }

    SetNode::~SetNode()
    {
        delete m_child;
        m_child = NULL;
    }

    //!Implementation of AbstractNode::get
    void SetNode::get( const AbstractResourceReader& /*in*/, AbstractResourceWriter* const out ) const
    {
        out->put( m_resourceID, m_valueToSet );
    }
    
    //!Implementation of AbstractNode::addChild
    bool SetNode::addChild( const QVariant& /*value*/, AbstractNode* child )
    {
        if( m_child )
            return false;
        m_child = child;
        return true;
    }

}
