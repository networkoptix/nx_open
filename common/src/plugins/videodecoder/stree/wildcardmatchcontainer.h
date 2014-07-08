////////////////////////////////////////////////////////////
// 2 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef WILDCARDMATCHCONTAINER_H
#define WILDCARDMATCHCONTAINER_H

#include <map>

#include <utils/match/wildcard.h>


namespace stree
{
    /*!
        Associative dictionary. Element key interpreted as wildcard mask. \a find methods finds first element with mask, supplying searched string
        \note Performs validation to mask by sequentially checking all elements. So do not use it when high performance is needed
    */
    template<class KeyType, class MappedType>
       class WildcardMatchContainer
    {
        typedef std::map<KeyType, MappedType> InternalContainerType;

    public:
        typedef typename InternalContainerType::iterator iterator;
        typedef typename InternalContainerType::const_iterator const_iterator;
        typedef typename InternalContainerType::key_type key_type;
        typedef typename InternalContainerType::mapped_type mapped_type;
        typedef typename InternalContainerType::value_type value_type;

        iterator begin()
        {
            return m_container.begin();
        }

        const_iterator begin() const
        {
            return m_container.begin();
        }

        iterator end()
        {
            return m_container.end();
        }

        const_iterator end() const
        {
            return m_container.end();
        }

        void erase( const iterator& pos )
        {
            m_container.erase( pos );
        }

        iterator find( const key_type& key )
        {
            for( typename InternalContainerType::iterator
                it = m_container.begin();
                it != m_container.end();
                ++it )
            {
                if( wildcardMatch( it->first, key ) )
                    return it;
            }

            return m_container.end();
        }

        const_iterator find( const key_type& key ) const
        {
            for( typename InternalContainerType::const_iterator
                it = m_container.begin();
                it != m_container.end();
                ++it )
            {
                if( wildcardMatch( it->first, key ) )
                    return it;
            }

            return m_container.end();
        }

        std::pair<iterator, bool> insert( const value_type& val )
        {
            return m_container.insert( val );
        }

    private:
        InternalContainerType m_container;
    };
}

#endif  //WILDCARDMATCHCONTAINER_H
