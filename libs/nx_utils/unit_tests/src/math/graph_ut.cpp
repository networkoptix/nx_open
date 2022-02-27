// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/math/graph.h>

namespace nx::utils::math::test {

class Graph:
    public ::testing::Test
{
protected:
    using IntGraph = math::Graph<int>;

    IntGraph buildRandomConnectedGraph()
    {
        const auto vertexCount = 2 + (::testing::UnitTest::GetInstance()->random_seed() % 17);

        IntGraph graph;
        // Connecting vertices to a line.
        connectVerticesToLine(&graph, 0, vertexCount);

        return graph;
    }

    IntGraph buildRandomNotConnectedGraph()
    {
        const auto vertexCount = 2 + (::testing::UnitTest::GetInstance()->random_seed() % 17);

        IntGraph graph;
        for (int i = 0; i < vertexCount; ++i)
            graph.addVertex(i);

        // The first vertex cannot be a disconnect point in the graph.
        const auto disconnectPoint =
            1 + ::testing::UnitTest::GetInstance()->random_seed() % (vertexCount - 1);
        // Vertices [0, disconnectPoint) and [disconnectPoint, vertexCount)
        // are the two connected components in the graph.

        connectVerticesToLine(&graph, 0, disconnectPoint);
        connectVerticesToLine(&graph, disconnectPoint, vertexCount);

        return graph;
    }

private:
    void connectVerticesToLine(IntGraph* graph, int start, int end)
    {
        for (int i = start; i < end - 1; ++i)
            graph->addEdge(i, i + 1);
    }
};

TEST_F(Graph, connected)
{
    ASSERT_TRUE(buildRandomConnectedGraph().connected());
    ASSERT_FALSE(buildRandomNotConnectedGraph().connected());
}

} // namespace nx::utils::math::test
