// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <queue>

namespace nx::utils::math {

/**
 * Undirected graph.
 * NOTE: Uses adjacency list to store vertices & edges.
 * TODO: #akolesnikov Add support for
 * std::is_move_constructible<Vertex>::value && !std::is_copy_constructible<Vertex>::value.
 */
template<typename Vertex>
// requires std::equally_comparable<Vertex>::value
class Graph
{
public:
    void addVertex(const Vertex& vertex)
    {
        m_outgoingEdges.emplace(vertex, Edges());
    }

    /**
     * Adds corresponding vertices if they do not exist.
     * No prior addVertex call is necessary.
     */
    void addEdge(const Vertex& from, const Vertex& to)
    {
        m_outgoingEdges[from].push_back(to);
        m_outgoingEdges[to].push_back(from);
    }

    /**
     * @return true if there a path from any vertex to any other vertex in the graph.
     */
    bool connected() const
    {
        if (m_outgoingEdges.empty())
            return true;

        std::unordered_set<Vertex> visitedVertices = breadthFirstSearch(
            m_outgoingEdges.begin()->first,
            [](auto&&...) {});

        return visitedVertices.size() == m_outgoingEdges.size();
    }

private:
    using Edges = std::vector<Vertex>;

    std::unordered_map<Vertex, Edges> m_outgoingEdges;

    template<typename BeforeVertexTraverseFunc>
    // requires std::is_invokable<BeforeVertexTraverseFunc, Vertex>::value
    std::unordered_set<Vertex> breadthFirstSearch(
        const Vertex& start,
        BeforeVertexTraverseFunc beforeVertexTraverseFunc) const
    {
        std::unordered_set<Vertex> visitedVertices;

        std::queue<Vertex> verticesToVisit;
        verticesToVisit.push(start);
        while (!verticesToVisit.empty())
        {
            auto vertex = std::move(verticesToVisit.front());
            verticesToVisit.pop();

            auto [vertexIter, inserted] = visitedVertices.emplace(std::move(vertex));
            if (!inserted)
                continue; //< The vertex has already been visited.
            const auto& currentVertex = *vertexIter;

            beforeVertexTraverseFunc(currentVertex);

            const auto it = m_outgoingEdges.find(currentVertex);
            if (it == m_outgoingEdges.end())
                continue; // There are no outgoing edges from the vertex.

            for (const auto& to: it->second)
            {
                if (visitedVertices.count(to) == 0)
                    verticesToVisit.push(to);
            }
        }

        return visitedVertices;
    }
};

} // namespace nx::utils::math
