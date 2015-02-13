/**********************************************************
* 13 feb 2015
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef NX_DIGRAPH_H
#define NX_DIGRAPH_H

#include <functional>
#include <map>
#include <limits>
#include <list>
#include <vector>

#include <boost/optional.hpp>


//!Directed graph
/*!
    \param NodeKey Requirements: comparable, copyable
    \note Class methods are not thread-safe
    \note No loops allowed
*/
template<class VerticeKey, class Comp = std::less<VerticeKey>>
class Digraph
{
public:
    enum Result
    {
        ok,
        alreadyExists,
        loopDetected,
        notFound
    };


    Digraph()
    :
        m_prevGivenVIndex( 0 )
    {
    }

    void addVertice( const VerticeKey& key )
    {
        getVerticeIndex( key );
    }

    //!Creates edge between two vertices. If either vertice does not exists, it is added first
    /*!
        \return \a ok or \a alreadyExists
    */
    Result addEdge( const VerticeKey& from, const VerticeKey& to )
    {
        const VIndex vFrom = getVerticeIndex( from );
        const VIndex vTo = getVerticeIndex( to );
        
        if( m_edges.size() <= vFrom )
            m_edges.resize( vFrom+1 );
        if( !m_edges[vFrom] )
            m_edges[vFrom] = Vertice( from, vFrom );

        if( m_edges[vFrom].get().edges.size() <= vTo )
            m_edges[vFrom].get().edges.resize( vTo+1 );
        if( m_edges[vFrom].get().edges[vTo] )
            return alreadyExists;   //edge is already there

        //detecting what-if loop
        std::list<VerticeKey> foundLoopPath;
        if( findAnyPath( to, from, &foundLoopPath ) == ok )
            return loopDetected;

        //adding new edge
        m_edges[vFrom].get().edges[vTo] = Vertice( to, vTo );
        return ok;
    }

    /*!
        \param foundPath In case of success, any path is stored here. \a from and \a to are included
        \return \a ok or \a notFound
    */
    Result findAnyPath(
        const VerticeKey& from,
        const VerticeKey& to,
        std::list<VerticeKey>* const foundPath ) const
    {
        const VIndex vFrom = getVerticeIndex( from );
        if( vFrom == npos )
            return notFound;
        const VIndex vTo = getVerticeIndex( to );
        if( vTo == npos )
            return notFound;

        const Result res = findAnyPathInternal( vFrom, vTo, foundPath );
        if( res != ok )
            return res;
        foundPath->push_front( from );
        return ok;
    }

private:
    typedef std::map<VerticeKey, size_t, Comp> VerticeKeyToIndexMap;
    typedef size_t VIndex;

    struct Vertice
    {
        VerticeKey key;
        VIndex index;
        std::vector<boost::optional<Vertice>> edges;

        Vertice()
        :
            index(std::numeric_limits<VIndex>::max())
        {
        }

        Vertice(
            const VerticeKey& _key,
            VIndex _index )
        :
            key(_key),
            index(_index)
        {
        }
    };

    static const VIndex npos = VIndex(-1);

    std::map<VerticeKey, VIndex, Comp> m_verticeKeyToIndex;
    std::vector<boost::optional<Vertice>> m_edges;
    VIndex m_prevGivenVIndex;

    VIndex getVerticeIndex( const VerticeKey& vKey ) const
    {
        auto iter = m_verticeKeyToIndex.find( vKey );
        return iter != m_verticeKeyToIndex.end() ? iter->second : npos;
    }

    VIndex getVerticeIndex( const VerticeKey& vKey )
    {
        auto iter = m_verticeKeyToIndex.lower_bound( vKey );
        if( iter == m_verticeKeyToIndex.end() || 
            (Comp()(iter->first, vKey) || Comp()(vKey, iter->first)) )    //iter.first != vKey
        {
            iter = m_verticeKeyToIndex.emplace_hint( iter, vKey, m_prevGivenVIndex++ );
        }
        return iter->second;
    }

    Result findAnyPathInternal(
        const VIndex& from,
        const VIndex& to,
        std::list<VerticeKey>* const foundPath ) const
    {
        if( (m_edges.size() <= from) || (!m_edges[from]) )
            return notFound;

        const std::vector<boost::optional<Vertice>>& startVerticeEdges = m_edges[from].get().edges;
        for( size_t i = 0; i < startVerticeEdges.size(); ++i )
        {
            if( !startVerticeEdges[i] )
                continue;

            if( (i == to) || (findAnyPathInternal( i, to, foundPath ) == ok) )
            {
                //saving path
                foundPath->push_front( startVerticeEdges[i].get().key );
                //unrolling recursive calls
                return ok;
            }
        }

        return notFound;
    }
};

#endif  //NX_DIGRAPH_H
