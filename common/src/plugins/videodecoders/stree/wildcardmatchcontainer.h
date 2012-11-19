////////////////////////////////////////////////////////////
// 2 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef WILDCARDMATCHCONTAINER_H
#define WILDCARDMATCHCONTAINER_H

#include <map>


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

        bool wildcardMatch( const QString& mask, const QString& str ) const
        {
            return wildcardMatch( mask.toAscii().data(), str.toAscii().data() );
        }

        //!Validates \a str for appliance to wild-card expression \a mask
        /*!
            \param mask Wildcard expression. Allowed to contain special symbols ? (any character except .), * (any number of any characters)
            \param str String being validated for appliance to \a mask
            \return true, if \a str has been validated with \a mask. false, otherwise
        */
        bool wildcardMatch( const char* mask, const char* str ) const
        {
            while( *str && *mask )
            {
                switch( *mask )
                {
                    case '*':
                    {
                        if( !*(mask+1) )
                            return true;    //we have * at the end
                        for( const char*
                            str1 = str;
                            *str1;
                            ++str1 )
                        {
                            if( wildcardMatch( mask+1, str1 ) )
                                return true;
                        }
                        return false;   //not validated
                    }
                    case '?':
                        if( *str == '.' )
                            return false;
                        break;

                    default:
                        if( *str != *mask )
                            return false;
                        break;
                }

                ++str;
                ++mask;
            }

            while( *mask )
            {
                if( *mask != '*' )
                    return false;
                ++mask;
            }

            if( *str )
                return false;

            return true;
        }
    };
}

#endif  //WILDCARDMATCHCONTAINER_H
