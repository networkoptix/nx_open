/**********************************************************
* 13 feb 2015
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef NX_DIGRAPH_H
#define NX_DIGRAPH_H

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <limits>
#include <list>
#include <set>
#include <unordered_map>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

//!Directed graph
/*!
    \param NodeKey Requirements: comparable, copyable
    \param VerticeKeyType Comparable type
    \param EdgeDataType Data stored with edge type
    NOTE: Class methods are not thread-safe
    NOTE: Edge loops are allowed
*/
template<class VerticeKeyType, class EdgeDataType, class Comp = std::less<VerticeKeyType>>
class Digraph
{
public:
    typedef VerticeKeyType vertice_key_type;
    typedef EdgeDataType edge_data_type;
    typedef EdgeDataType* edge_data_pointer;

    Digraph()
    {
    }

    template<class VerticeOneRef>
    void addVertice( VerticeOneRef&& key )
    {
        getOrAddVertice( std::forward<VerticeOneRef>(key) );
    }

    template<class VerticeOneRef>
    void removeVertice( const VerticeOneRef& key )
    {
        m_keyToVertice.erase( key );
    }

    //!Creates edge between two vertices. If either vertice does not exists, it is added first
    /*!
        \return true if added, false if edge already exists
        NOTE: Loops are allowed
    */
    template<
        class VerticeOneRef,
        class VerticeTwoRef,
        class EdgeDataRef>
    std::pair<edge_data_type*, bool> addEdge(
        VerticeOneRef&& startVerticeKey,
        VerticeTwoRef&& endVerticeKey,
        EdgeDataRef&& edgeData )
    {
        Vertice* vFrom = getOrAddVertice( std::forward<VerticeOneRef>(startVerticeKey) );
        Vertice* vTo = getOrAddVertice( std::forward<VerticeTwoRef>(endVerticeKey) );

        auto edgeInsertionPair = vFrom->edges.emplace( vTo, std::forward<EdgeDataRef>(edgeData) );
        if( edgeInsertionPair.second )
            vTo->incomingEdges.insert( vFrom ); //added new edge, adding back path
        return std::pair<edge_data_type*, bool>(
            &edgeInsertionPair.first->second,
            edgeInsertionPair.second );
    }

    bool findEdge(
        const vertice_key_type& from,
        const vertice_key_type& to,
        const edge_data_type** edgeData ) const
    {
        auto vFromIter = m_keyToVertice.find( from );
        if( vFromIter == m_keyToVertice.end() )
            return false;
        auto vToIter = m_keyToVertice.find( to );
        if( vToIter == m_keyToVertice.end() )
            return false;

        auto edgeIter = vFromIter->second->edges.find( vToIter->second.get() );
        if( edgeIter == vFromIter->second->edges.end() )
            return false;

        *edgeData = &edgeIter->second;
        return true;
    }

    /*!
        \param foundPath In case of success, vertices along found path are stored here.
            from and to are included. Can be nullptr
    */
    bool findAnyPath(
        const vertice_key_type& from,
        const vertice_key_type& to,
        std::list<vertice_key_type>* const foundPath,
        std::list<edge_data_type>* const edgesTravelled = nullptr ) const
    {
        return findAnyPathIf(
            from,
            to,
            foundPath,
            edgesTravelled,
            []( const edge_data_type& ){ return true; } );
    }

    //!Finds any path between two vertices using only edges for which edgeSelectionPred returns true
    template<class EdgeSelectionPredicateType>
    bool findAnyPathIf(
        const vertice_key_type& from,
        const vertice_key_type& to,
        std::list<vertice_key_type>* const foundPath,
        std::list<edge_data_type>* const edgesTravelled,
        EdgeSelectionPredicateType edgeSelectionPred ) const
    {
        const Vertice* vFrom = findVertice( from );
        if( vFrom == nullptr )
            return false;
        const Vertice* vTo = findVertice( to );
        if( vTo == nullptr )
            return false;

        std::deque<const Vertice*> pathTravelled;
        const bool res = findAnyPathInternal(
            vFrom, vTo, foundPath, edgesTravelled, &pathTravelled, edgeSelectionPred );
        if( res && foundPath )
            foundPath->push_front( vFrom->key );  //findAnyPathInternal does not add start vertice to the path
        return res;
    }

private:
    struct Vertice;

    //!map<vertice key, vertice>
    typedef std::map<vertice_key_type, std::unique_ptr<Vertice>, Comp> KeyToVerticeDictionary;
    //!map<end vertice, edge>
    typedef std::unordered_map<Vertice*, EdgeDataType> EdgeDictionary;

    struct Vertice
    {
        vertice_key_type key;
        //!Edges, outgoing from this vertice
        EdgeDictionary edges;
        //!Vertices, having edges to this vertice
        std::set<Vertice*> incomingEdges;

        Vertice()
        {
        }

        template<class VerticeKeyRef>
        Vertice( VerticeKeyRef&& _key )
        :
            key( std::forward<VerticeKeyRef>(_key) )
        {
        }

        ~Vertice()
        {
            for( std::pair<Vertice* const, EdgeDataType>& edge: edges )
                edge.first->incomingEdges.erase( this );
            for( Vertice* const vertice: incomingEdges )
                vertice->edges.erase( this );
        }
    };

    KeyToVerticeDictionary m_keyToVertice;


    const Vertice* findVertice( const vertice_key_type& vKey ) const
    {
        auto iter = m_keyToVertice.find( vKey );
        return iter != m_keyToVertice.end()
            ? iter->second.get()
            : nullptr;
    }

    template<class VerticeKeyRef>
    Vertice* getOrAddVertice( VerticeKeyRef&& vKey )
    {
        auto it = m_keyToVertice.emplace(
            vKey,
            std::unique_ptr<Vertice>() ).first;
        if( !it->second )
            it->second.reset( new Vertice( std::forward<VerticeKeyRef>(vKey) ) );
        return it->second.get();
    }

    template<class EdgeSelectionPredicateType>
    bool findAnyPathInternal(
        const Vertice* from,
        const Vertice* to,
        std::list<vertice_key_type>* const foundPath,
        std::list<edge_data_type>* const edgesTravelled,
        std::deque<const Vertice*>* const pathTravelled,
        EdgeSelectionPredicateType edgeSelectionPred ) const
    {
        pathTravelled->push_back( from );

        for( const typename std::pair<Vertice* const, EdgeDataType>& edge: from->edges )
        {
            if( !edgeSelectionPred( edge.second ) )
                continue;

            bool result = false;
            if( edge.first == to )
            {
                result = true;  //found path
            }
            else if( std::find(     //detecting path loops to prevent infinite recursion
                        pathTravelled->cbegin(),
                        pathTravelled->cend(),
                        edge.first ) != pathTravelled->cend() )
            {
                result = false;   //loop detected, not found
            }
            else
            {
                result = findAnyPathInternal(
                    edge.first, to,
                    foundPath, edgesTravelled, pathTravelled,
                    edgeSelectionPred );
            }

            if( result )
            {
                if( foundPath )
                    foundPath->push_front( edge.first->key );    //reached target, saving path found
                if( edgesTravelled )
                    edgesTravelled->push_front( edge.second );
            }
            //unrolling recursive calls
            if( result )
            {
                pathTravelled->pop_back();
                return true;    //found some path
            }
        }

        pathTravelled->pop_back();
        return false;
    }
};

#endif  //NX_DIGRAPH_H
