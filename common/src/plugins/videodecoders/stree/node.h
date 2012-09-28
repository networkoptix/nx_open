////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACTNODE_H
#define ABSTRACTNODE_H

#include "resourcecontainer.h"


namespace stree
{
    //!Base class for all nodes
    class AbstractNode
    {
    public:
        //!Do node processing. This method can put some value to \a out and/or scall \a get of some child node
        virtual void get( const AbstractResourceReader& in, AbstractResourceWriter* const out ) const = 0;
        //!Add child node. Takes ownership of \a child in case of success
        /*!
            Implementation is allowed to reject adding child. In this case it must return false.
            If noded added, true is returned and ownership of \a child object is taken
        */
        virtual bool addChild( const QVariant& value, AbstractNode* child ) = 0;
    };

    //!Iterates through all its children
    class SequenceNode
    :
        public AbstractNode
    {
    public:
    };

    //!Chooses child node to follow based on some condition
    class ConditionNode
    :
        public AbstractNode
    {
    public:
    };

    //!Puts some resource value to output
    class SetNode
    :
        public AbstractNode
    {
    public:
    };
}

#endif  //ABSTRACTNODE_H
