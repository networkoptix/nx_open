////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACTNODE_H
#define ABSTRACTNODE_H

#include "resourcecontainer.h"

#include <utils/common/log.h>


/*!
    Contains implementation of simple search tree
*/
namespace stree
{
    //!Base class for all nodes
    /*!
        \note Descendant is not required to be thread-safe
    */
    class AbstractNode
    {
    public:
        virtual ~AbstractNode() {}

        //!Do node processing. This method can put some value to \a out and/or call \a get of some child node
        virtual void get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const = 0;
        //!Add child node. Takes ownership of \a child in case of success
        /*!
            Implementation is allowed to reject adding child. In this case it must return false.
            If noded added, true is returned and ownership of \a child object is taken
        */
        virtual bool addChild( const QVariant& value, std::unique_ptr<AbstractNode> child ) = 0;
    };

    //!Iterates through all its children in order, defined by child value, cast to signed int
    /*!
        \note Children with same value are iterated in order they were added
    */
    class SequenceNode
    :
        public AbstractNode
    {
    public:
        //!Implementation of AbstractNode::get
        virtual void get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const;
        //!Implementation of AbstractNode::addChild
        virtual bool addChild( const QVariant& value, std::unique_ptr<AbstractNode> child );

    private:
        std::multimap<int, std::unique_ptr<AbstractNode>> m_children;
    };

    //!Chooses child node to follow based on some condition
    /*!
        All children have unique values. Attempt to add children with existing value will fail
        \param ConditionContainer must be associative dictionary from std::pair<Value, AbstractNode*>
        \a ConditionContainer::mapped_type MUST be AbstractNode*

        example of \a ConditionContainer is std::map
    */
    template<typename Key, template<typename, typename> class ConditionContainer>
    class ConditionNode
    :
        public AbstractNode
    {
        typedef ConditionContainer<Key, std::unique_ptr<AbstractNode>> Container;

    public:
        ConditionNode( int matchResID )
        :
            m_matchResID( matchResID )
        {
        }

        //!Implementation of AbstractNode::get
        virtual void get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const
        {
            NX_LOG( lit("Stree. Condition. Selecting child by resource %1").arg(m_matchResID), cl_logDEBUG2 );

            QVariant value;
            if( !in.get( m_matchResID, &value ) )
            {
                NX_LOG( lit("Stree. Condition. Resource (%1) not found in input data").arg(m_matchResID), cl_logDEBUG2 );
                return;
            }

            const typename Container::key_type& typedValue = value.value<typename Container::key_type>();
            typename Container::const_iterator it = m_children.find( typedValue );
            if( it == m_children.end() )
            {
                NX_LOG( lit("Stree. Condition. Could not find child by value %1").arg(value.toString()), cl_logDEBUG2 );
                return;
            }

            NX_LOG( lit("Stree. Condition. Found child with value %1 by search value %2").arg(it->first).arg(value.toString()), cl_logDEBUG2 );
            it->second->get( in, out );
        }

        //!Implementation of AbstractNode::addChild
        virtual bool addChild( const QVariant& value, std::unique_ptr<AbstractNode> child )
        {
            return m_children.emplace( value.value<typename Container::key_type>(), std::move(child) ).second;
        }

    private:
        Container m_children;
        int m_matchResID;
    };

    //!Checks presense of specified resource in input container
    /*!
        Allows only 2 children: \a false and \a true
    */
    class ResPresenceNode
    :
        public AbstractNode
    {
    public:
        ResPresenceNode( int matchResID );

        //!Implementation of AbstractNode::get
        virtual void get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const;
        //!Implementation of AbstractNode::addChild
        virtual bool addChild( const QVariant& value, std::unique_ptr<AbstractNode> child );

    private:
        //[0] - for \a false. [1] - for \a true
        std::unique_ptr<AbstractNode> m_children[2];
        int m_matchResID;
    };

    //!Puts some resource value to output
    class SetNode
    :
        public AbstractNode
    {
    public:
        SetNode(
            int resourceID,
            const QVariant& valueToSet );

        //!Implementation of AbstractNode::get
        /*!
            Adds to \a out resource and then calls \a m_child->get() (if \a child exists)
        */
        virtual void get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const;
        //!Implementation of AbstractNode::addChild
        virtual bool addChild( const QVariant& value, std::unique_ptr<AbstractNode> child );

    private:
        std::unique_ptr<AbstractNode> m_child;
        int m_resourceID;
        const QVariant m_valueToSet;
    };

}

#endif  //ABSTRACTNODE_H
