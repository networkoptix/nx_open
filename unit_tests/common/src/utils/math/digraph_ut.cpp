/**********************************************************
* 13 feb 2015
* akolesnikov@networkoptix.com
***********************************************************/

#include <gtest/gtest.h>

#include <array>
#include <algorithm>

#include <utils/math/digraph.h>


namespace
{
    std::list<int> generatePath( int verticeArray[], const size_t arraySize )
    {
        std::list<int> l;
        std::copy( &verticeArray[0], &verticeArray[0]+arraySize, std::back_inserter(l) );
        return l;
    }
}

TEST( Digraph, findAnyPath )
{
    Digraph<int> graph;

    //graph.addVertice( 1 );
    //graph.addVertice( 2 );
    //graph.addVertice( 3 );
    //graph.addVertice( 4 );

    ASSERT_EQ( graph.addEdge( 1, 2 ), Digraph<int>::ok );
    ASSERT_EQ( graph.addEdge( 2, 3 ), Digraph<int>::ok );
    ASSERT_EQ( graph.addEdge( 3, 4 ), Digraph<int>::ok );
    ASSERT_EQ( graph.addEdge( 3, 4 ), Digraph<int>::alreadyExists );

    std::list<int> path;
    {
        path.clear();
        ASSERT_EQ( graph.findAnyPath( 1, 4, &path ), Digraph<int>::ok );
        int answer[] = { 1, 2, 3, 4 };
        ASSERT_EQ( path, generatePath( answer, sizeof(answer)/sizeof(*answer) ) );
    }
    {
        path.clear();
        ASSERT_EQ( graph.findAnyPath( 1, 3, &path ), Digraph<int>::ok );
        int answer[] = { 1, 2, 3 };
        ASSERT_EQ( path, generatePath( answer, sizeof(answer)/sizeof(*answer) ) );
    }
    {
        path.clear();
        ASSERT_EQ( graph.findAnyPath( 1, 2, &path ), Digraph<int>::ok );
        int answer[] = { 1, 2 };
        ASSERT_EQ( path, generatePath( answer, sizeof(answer)/sizeof(*answer) ) );
    }

    ASSERT_EQ( graph.findAnyPath( 4, 2, &path ), Digraph<int>::notFound );
    ASSERT_EQ( graph.findAnyPath( 3, 1, &path ), Digraph<int>::notFound );
    ASSERT_EQ( graph.findAnyPath( 1, 1, &path ), Digraph<int>::notFound );
}

TEST( Digraph, loopDetection )
{
    Digraph<int> graph;

    ASSERT_EQ( graph.addEdge( 1, 2 ), Digraph<int>::ok );
    ASSERT_EQ( graph.addEdge( 2, 3 ), Digraph<int>::ok );
    ASSERT_EQ( graph.addEdge( 3, 4 ), Digraph<int>::ok );
    ASSERT_EQ( graph.addEdge( 4, 1 ), Digraph<int>::loopDetected );
}
