////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACTNODE_H
#define ABSTRACTNODE_H

#include "resourcecontainer.h"


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
        virtual bool addChild( const QVariant& value, AbstractNode* child ) = 0;
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
        virtual ~SequenceNode();

        //!Implementation of AbstractNode::get
        virtual void get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const;
        //!Implementation of AbstractNode::addChild
        virtual bool addChild( const QVariant& value, AbstractNode* child );

    private:
        std::multimap<int, AbstractNode*> m_children;
    };

    //!Chooses child node to follow based on some condition
    /*!
        All children have unique values. Attempt to add children with existing value will fail
        \param ConditionContainer must be associative dictionary from std::pair<Value, AbstractNode*>
        \a ConditionContainer::mapped_type MUST be AbstractNode*

        example of \a ConditionContainer is std::map
    */
    template<typename ConditionContainer>
    class ConditionNode
    :
        public AbstractNode
    {
    public:
        ConditionNode( int matchResID )
        :
            m_matchResID( matchResID )
        {
        }

        virtual ~ConditionNode()
        {
            for( ConditionContainer::iterator
                it = m_children.begin();
                it != m_children.end();
                )
            {
                delete it->second;
                m_children.erase( it++ );
            }
        }

        //!Implementation of AbstractNode::get
        virtual void get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const
        {
            QVariant value;
            if( !in.get( m_matchResID, &value ) )
                return;

            const ConditionContainer::key_type& typedValue = value.value<ConditionContainer::key_type>();
            ConditionContainer::const_iterator it = m_children.find( typedValue );
            if( it == m_children.end() )
                return;

            it->second->get( in, out );
        }

        //!Implementation of AbstractNode::addChild
        virtual bool addChild( const QVariant& value, AbstractNode* child )
        {
            return m_children.insert( std::make_pair( value.value<typename ConditionContainer::key_type>(), child ) ).second;
        }

    private:
        ConditionContainer m_children;
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
        virtual ~SetNode();

        //!Implementation of AbstractNode::get
        /*!
            Adds to \a out resource and then calls \a m_child->get() (if \a child exists)
        */
        virtual void get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const;
        //!Implementation of AbstractNode::addChild
        virtual bool addChild( const QVariant& value, AbstractNode* child );

    private:
        AbstractNode* m_child;
        int m_resourceID;
        const QVariant m_valueToSet;
    };

    namespace MatchType
    {
        enum Value
        {
            unknown,
            equal,
            wildcard
        };

        Value fromString( const QString& str );
    };
}

#endif  //ABSTRACTNODE_H
