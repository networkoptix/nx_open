#include <gtest/gtest.h>

#include <array>
#include <algorithm>

#include <nx/utils/math/digraph.h>
#include <nx/utils/thread/mutex_lock_analyzer.h>

namespace
{
    template<typename T, size_t arraySize>
    std::list<T> generatePath( const T(&verticeArray)[arraySize] )
    {
        std::list<T> l;
        std::copy( &verticeArray[0], &verticeArray[0]+arraySize, std::back_inserter(l) );
        return l;
    }
}

namespace nx {
namespace utils {

TEST( Digraph, findAnyPath )
{
    Digraph<int, int> graph;

    //graph.addVertice( 1 );
    //graph.addVertice( 2 );
    //graph.addVertice( 3 );
    //graph.addVertice( 4 );

    ASSERT_TRUE( graph.addEdge( 1, 2, 1 ).second );
    ASSERT_TRUE( graph.addEdge( 2, 3, 2 ).second );
    ASSERT_TRUE( graph.addEdge( 3, 4, 3 ).second );
    ASSERT_FALSE( graph.addEdge( 3, 4, 4 ).second );

    std::list<int> path;
    {
        path.clear();
        ASSERT_TRUE( graph.findAnyPath( 1, 4, &path ) );
        const int correctPath[] = { 1, 2, 3, 4 };
        ASSERT_EQ( path, generatePath( correctPath ) );
    }
    {
        path.clear();
        ASSERT_TRUE( graph.findAnyPath( 1, 3, &path ) );
        const int correctPath[] = { 1, 2, 3 };
        ASSERT_EQ( path, generatePath( correctPath ) );
    }
    {
        path.clear();
        ASSERT_TRUE( graph.findAnyPath( 1, 2, &path ) );
        const int correctPath[] = { 1, 2 };
        ASSERT_EQ( path, generatePath( correctPath ) );
    }

    ASSERT_FALSE( graph.findAnyPath( 4, 2, &path ) );
    ASSERT_FALSE( graph.findAnyPath( 3, 1, &path ) );
    ASSERT_FALSE( graph.findAnyPath( 1, 1, &path ) );
}

TEST( Digraph, loopDetection )
{
    Digraph<int, int> graph;

    ASSERT_TRUE( graph.addEdge( 1, 2, 1 ).second );
    ASSERT_TRUE( graph.addEdge( 2, 3, 2 ).second );
    ASSERT_TRUE( graph.addEdge( 3, 4, 3 ).second );
    ASSERT_TRUE( graph.addEdge( 4, 1, 4 ).second );
    ASSERT_TRUE( graph.addEdge( 1, 5, 5 ).second );
    ASSERT_TRUE( graph.addEdge( 5, 6, 6 ).second );
    ASSERT_TRUE( graph.addEdge( 7, 7, 7 ).second );
    
    {
        std::list<int> path;
        std::list<int> edgeTravelled;

        ASSERT_TRUE( graph.findAnyPath( 1, 6, &path, &edgeTravelled ) );

        const int correctPath[] = { 1, 5, 6 };
        ASSERT_EQ( path, generatePath( correctPath ) );

        const int edges[] = { 5, 6 };
        ASSERT_EQ( edgeTravelled, generatePath( edges ) );
    }

    {
        std::list<int> path;
        std::list<int> edgeTravelled;

        ASSERT_TRUE( graph.findAnyPath( 3, 2, &path, &edgeTravelled ) );

        const int correctPath[] = { 3, 4, 1, 2 };
        ASSERT_EQ( path, generatePath( correctPath ) );

        const int edges[] = { 3, 4, 1 };
        ASSERT_EQ( edgeTravelled, generatePath( edges ) );
    }

    {
        std::list<int> path;

        ASSERT_FALSE( graph.findAnyPath( 7, 1, &path ) );
    }

    {
        std::list<int> path;
        std::list<int> edgeTravelled;

        ASSERT_TRUE( graph.findAnyPath( 7, 7, &path, &edgeTravelled ) );

        const int correctPath[] = { 7, 7 };
        ASSERT_EQ( path, generatePath( correctPath ) );

        const int edges[] = { 7 };
        ASSERT_EQ( edgeTravelled, generatePath( edges ) );
    }


    //removing vertice

    graph.removeVertice( 1 );
    {
        std::list<int> path;

        ASSERT_FALSE( graph.findAnyPath( 7, 1, &path ) );
        ASSERT_FALSE( graph.findAnyPath( 1, 2, &path ) );
        ASSERT_FALSE( graph.findAnyPath( 4, 1, &path ) );
        ASSERT_FALSE( graph.findAnyPath( 2, 5, &path ) );
    }

    {
        std::list<int> path;
        std::list<int> edgeTravelled;

        ASSERT_TRUE( graph.findAnyPath( 2, 4, &path, &edgeTravelled ) );

        const int correctPath[] = { 2, 3, 4 };
        ASSERT_EQ( path, generatePath( correctPath ) );

        const int edges[] = { 2, 3 };
        ASSERT_EQ( edgeTravelled, generatePath( edges ) );
    }
}

TEST( Digraph, are_elements_same_in_sorted_ranges )
{
    {
        const int range1[] = { 2, 3, 4 };
        const int range2[] = { 2, 2, 3, 3, 4, 4 };

        auto r1 = generatePath( range1 );
        auto r2 = generatePath( range2 );

        ASSERT_TRUE( are_elements_same_in_sorted_ranges(
            r1.begin(), r1.end(),
            r2.begin(), r2.end() ) );
    }

    {
        const int range1[] = { 2, 2, 3, 3, 4, 4 };
        const int range2[] = { 2, 2, 3, 3, 4, 4 };

        auto r1 = generatePath( range1 );
        auto r2 = generatePath( range2 );

        ASSERT_TRUE( are_elements_same_in_sorted_ranges(
            r1.begin(), r1.end(),
            r2.begin(), r2.end() ) );
    }

    {
        const int range1[] = { 1 };
        const int range2[] = { 1, 1, 1, 1, 1, 1 };

        auto r1 = generatePath( range1 );
        auto r2 = generatePath( range2 );

        ASSERT_TRUE( are_elements_same_in_sorted_ranges(
            r1.begin(), r1.end(),
            r2.begin(), r2.end() ) );
    }

    {
        const int range1[] = { 1, 2, 3, 4 };
        const int range2[] = { 1, 2, 3, 4 };

        auto r1 = generatePath( range1 );
        auto r2 = generatePath( range2 );

        ASSERT_TRUE( are_elements_same_in_sorted_ranges(
            r1.begin(), r1.end(),
            r2.begin(), r2.end() ) );
    }

    {
        const int range1[] = { 1 };
        const int range2[] = { 1 };

        auto r1 = generatePath( range1 );
        auto r2 = generatePath( range2 );

        ASSERT_TRUE( are_elements_same_in_sorted_ranges(
            r1.begin(), r1.end(),
            r2.begin(), r2.end() ) );
    }

    {
        const int range1[] = { 1 };
        const int range2[] = { 2 };

        auto r1 = generatePath( range1 );
        auto r2 = generatePath( range2 );

        ASSERT_FALSE( are_elements_same_in_sorted_ranges(
            r1.begin(), r1.end(),
            r2.begin(), r2.end() ) );
    }

    {
        const int range1[] = { 2, 3, 4, 1 };
        const int range2[] = { 2, 2, 3, 3, 4, 4 };

        auto r1 = generatePath( range1 );
        auto r2 = generatePath( range2 );

        ASSERT_FALSE( are_elements_same_in_sorted_ranges(
            r1.begin(), r1.end(),
            r2.begin(), r2.end() ) );
    }

    {
        const int range1[] = { 1, 2, 2, 3, 3, 4, 4 };
        const int range2[] = { 2, 2, 3, 3, 4, 4 };

        auto r1 = generatePath( range1 );
        auto r2 = generatePath( range2 );

        ASSERT_FALSE( are_elements_same_in_sorted_ranges(
            r1.begin(), r1.end(),
            r2.begin(), r2.end() ) );
    }

    {
        const int range1[] = { 2, 3, 4 };
        const int range2[] = { 1, 2, 2, 3, 3, 4, 4 };

        auto r1 = generatePath( range1 );
        auto r2 = generatePath( range2 );

        ASSERT_FALSE( are_elements_same_in_sorted_ranges(
            r1.begin(), r1.end(),
            r2.begin(), r2.end() ) );
    }

    {
        const int range1[] = { 2, 2, 3, 3, 4, 4 };
        const int range2[] = { 2, 2, 3, 3, 4, 4, 6 };

        auto r1 = generatePath( range1 );
        auto r2 = generatePath( range2 );

        ASSERT_FALSE( are_elements_same_in_sorted_ranges(
            r1.begin(), r1.end(),
            r2.begin(), r2.end() ) );
    }
}

} // namespace utils
} // namespace nx
