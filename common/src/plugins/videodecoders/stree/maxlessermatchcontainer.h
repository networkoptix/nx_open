////////////////////////////////////////////////////////////
// 17 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef MAXLESSERMATCHCONTAINER_H
#define MAXLESSERMATCHCONTAINER_H

#include <map>


namespace stree
{
    /*!
        \a find methods select element with maximum key less than being searched
    */
    template<class KeyType, class MappedType>
       class MaxLesserMatchContainer
    {
        typedef std::map<KeyType, MappedType, std::greater<KeyType> > InternalContainerType;

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
            return m_container.upper_bound( key );
        }

        const_iterator find( const key_type& key ) const
        {
            return m_container.upper_bound( key );
        }

        std::pair<iterator, bool> insert( const value_type& val )
        {
            return m_container.insert( val );
        }

    private:
        InternalContainerType m_container;
    };
}

#endif  //MAXLESSERMATCHCONTAINER_H
