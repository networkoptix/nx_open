// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/to_string.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/analytics/analytics_entities_tree.h>

namespace nx::vms::client::desktop {

void PrintTo(core::AnalyticsEntitiesTreeBuilder::NodeType nodeType, ::std::ostream* os)
{
    *os << nx::reflect::toString(nodeType);
}

namespace test {

using Node = core::AnalyticsEntitiesTreeBuilder::Node;
using NodeFilter = core::AnalyticsEntitiesTreeBuilder::NodeFilter;
using NodeType = core::AnalyticsEntitiesTreeBuilder::NodeType;
using NodePtr = core::AnalyticsEntitiesTreeBuilder::NodePtr;

/** Helper class to display tree differences in console. */
struct TreeComparator
{
    explicit TreeComparator(NodePtr node):
        node(node)
    {
    }

    bool operator==(const TreeComparator& other) const
    {
        auto compareNode = nx::utils::y_combinator(
            [](auto compareNode, NodePtr actual, NodePtr expected) -> bool
            {
                if (actual->nodeType != expected->nodeType)
                    return false;

                if (actual->text != expected->text)
                    return false;

                if (actual->entityId != expected->entityId)
                    return false;

                if (actual->children.size() != expected->children.size())
                    return false;

                for (size_t i = 0; i < actual->children.size(); ++i)
                {
                    if (!compareNode(actual->children[i], expected->children[i]))
                        return false;
                }
                return true;
            });
        return compareNode(node, other.node);
    }

    const NodePtr node;
};

void PrintTo(TreeComparator comparator, ::std::ostream* os)
{
    *os << std::endl;
    auto printNode = nx::utils::y_combinator(
        [os](auto printNode, NodePtr node, std::string offset) -> void
        {
            *os << offset << "[" << nx::reflect::toString(node->nodeType) << "]"
                << node->text.toStdString()
                << " (" << node->entityId.toStdString() << ")" << std::endl;
            for (auto child: node->children)
                printNode(child, offset + "    ");
        });
    printNode(comparator.node, std::string());
}

namespace builder {

NodePtr node(
    NodeType nodeType,
    QString text,
    Node::EntityTypeId entityId,
    std::vector<NodePtr> children = {})
{
    NodePtr result(new Node(nodeType, {}, text));
    result->entityId = entityId;
    result->children = children;
    return result;
}

NodePtr root(std::vector<NodePtr> children)
{
    return node(NodeType::root, "", "", children);
}

NodePtr engine(QString text, Node::EntityTypeId entityId, std::vector<NodePtr> children)
{
    return node(NodeType::engine, text, entityId, children);
}

NodePtr group(QString text, Node::EntityTypeId entityId, std::vector<NodePtr> children)
{
    return node(NodeType::group, text, entityId, children);
}

NodePtr eventType(QString text, Node::EntityTypeId entityId)
{
    return node(NodeType::eventType, text, entityId);
}

} // namespace builder

class AnalyticsEntitiesTreeBuilderTest: public ::testing::Test
{
protected:
    void givenTree(NodePtr tree) { m_sourceTree = tree; }

    void whenCleanupTree()
    {
        m_targetTree = core::AnalyticsEntitiesTreeBuilder::cleanupTree(m_sourceTree);
    }

    void whenFilterTreeExclusive(NodeFilter excludeNode)
    {
        m_targetTree = core::AnalyticsEntitiesTreeBuilder::filterTreeExclusive(m_sourceTree, excludeNode);
    }

    void whenFilterTreeInclusive(NodeFilter includeNode)
    {
        m_targetTree = core::AnalyticsEntitiesTreeBuilder::filterTreeInclusive(m_sourceTree, includeNode);
    }

    void expectTree(NodePtr expectedTree)
    {
        ASSERT_TRUE(!expectedTree.isNull());
        ASSERT_TRUE(!m_targetTree.isNull());
        ASSERT_EQ(TreeComparator(expectedTree), TreeComparator(m_targetTree));

    }

private:
    NodePtr m_sourceTree;
    NodePtr m_targetTree;
};

TEST_F(AnalyticsEntitiesTreeBuilderTest, filterSingleEngine)
{
    using namespace builder;
    givenTree(
        engine("Engine 1", "engine1", {
            group("Group 1", "group1", {
                eventType("Event 1", "event1"),
                eventType("", "<invalid>")
            }),
            group("Group 2", "group2", {
                eventType("", "<invalid>")
            }),
            eventType("Event 2", "event2"),
            eventType("", "<invalid>")
        }));

    // Filter out event type nodes with empty text.
    whenFilterTreeExclusive(
        [](NodePtr node) { return node->nodeType == NodeType::eventType && node->text.isEmpty(); });

    // Filtered tree must still have an engine as the root. Only Group 1 and Event 2 should be kept.
    // Group 2 will be removed as an empty group.
    expectTree(
        engine("Engine 1", "engine1", {
            group("Group 1", "group1", {
                eventType("Event 1", "event1")
            }),
            eventType("Event 2", "event2"),
        }));
}

TEST_F(AnalyticsEntitiesTreeBuilderTest, filterOutEmptyEngines)
{
    using namespace builder;
    givenTree(
        root({
            engine("Engine", "engine", {
                group("Group", "group", {})
            })
        }));
    whenCleanupTree();
    expectTree(root({}));
}

TEST_F(AnalyticsEntitiesTreeBuilderTest, pullOutSingleEngine)
{
    using namespace builder;
    givenTree(
        root({
            engine("Engine", "engine", {
                eventType("Event", "event")
            })
        }));
    whenCleanupTree();
    expectTree(
        engine("Engine", "engine", {
            eventType("Event", "event")
        }));
}

TEST_F(AnalyticsEntitiesTreeBuilderTest, filterInclusiveGroup)
{
    using namespace builder;
    givenTree(
        engine("Engine 1", "engine1", {
            group("Sound-related events", "engine1.sound", {
                    eventType("Gunshot", "engine1.sound.gunshot"),
                    eventType("Beep", "engine1.sound.beep")
                }),
            group("Face detection", "faceDetection", {
                    eventType("Human face", "faceDetection.human"),
                    eventType("Alien face", "engine1.alien")
                }),
            eventType("Line Crossing", "engine1.lineCrossing"),
            eventType("Object in the area", "engine1.objectInTheArea")
        }));

    QSet<Node::EntityTypeId> allowedEntites;
    allowedEntites.insert("engine1.sound");
    allowedEntites.insert("engine1.objectInTheArea");

    whenFilterTreeInclusive(
        [allowedEntites](NodePtr node) { return allowedEntites.contains(node->entityId); });

    expectTree(
        engine("Engine 1", "engine1", {
            group("Sound-related events", "engine1.sound", {
                    eventType("Gunshot", "engine1.sound.gunshot"),
                    eventType("Beep", "engine1.sound.beep")
                }),
            eventType("Object in the area", "engine1.objectInTheArea")
        })
    );
}

TEST_F(AnalyticsEntitiesTreeBuilderTest, cleanupNestedGroup)
{
    using namespace builder;
    givenTree(
        root({
            engine("Engine 1", "engine1", {
                group("Group 1", "engine1.group1", {
                    group("Group 2", "engine1.group1.group2", {}),
                })
            }),
            engine("Engine 2", "engine2", {
                group("Group 1", "engine2.group1", {
                    group("Group 2", "engine2.group1.group2", {
                        eventType("Object in the area", "engine1.objectInTheArea")
                    })
                })
            })
        })
    );

    whenCleanupTree();

    expectTree(
        engine("Engine 2", "engine2", {
            group("Group 1", "engine2.group1", {
                group("Group 2", "engine2.group1.group2", {
                    eventType("Object in the area", "engine1.objectInTheArea")
                })
            })
        })
    );
}

TEST_F(AnalyticsEntitiesTreeBuilderTest, filterNestedGroupExclusive)
{
    using namespace builder;
    givenTree(
        root({
            engine("Engine 1", "engine1", {
                group("Sound-related events", "engine1.sound", {
                    eventType("", "engine1.sound.gunshot"),
                    eventType("Beep", "engine1.sound.beep"),
                    group("Face detection", "faceDetection", {
                        eventType("", "faceDetection.human"),
                        eventType("Alien face", "engine1.alien")
                    })
                }),
                eventType("Line Crossing", "lineCrossing"),
                eventType("Object in the area", "engine1.objectInTheArea")
            }),
            engine("Engine 2", "engine2", {
                group("Sound-related events", "engine2.sound", {
                    eventType("", "engine2.sound.gunshot"),
                    eventType("", "engine2.sound.beep"),
                    group("Face detection", "faceDetection", {
                        eventType("", "faceDetection.human"),
                        eventType("", "engine2.alien")
                    })
                }),
                eventType("", "lineCrossing"),
                eventType("Object in the area", "engine2.objectInTheArea")
            })
        })
    );

    // Filter out event type nodes with empty text.
    whenFilterTreeExclusive(
        [](NodePtr node) { return node->nodeType == NodeType::eventType && node->text.isEmpty(); });

    expectTree(
        root({
            engine("Engine 1", "engine1", {
                group("Sound-related events", "engine1.sound", {
                    eventType("Beep", "engine1.sound.beep"),
                    group("Face detection", "faceDetection", {
                        eventType("Alien face", "engine1.alien")
                    })
                }),
                eventType("Line Crossing", "lineCrossing"),
                eventType("Object in the area", "engine1.objectInTheArea")
            }),
            engine("Engine 2", "engine2", {
                eventType("Object in the area", "engine2.objectInTheArea")
            })
        })
    );
}

TEST_F(AnalyticsEntitiesTreeBuilderTest, filterNestedGroupInclusive)
{
    using namespace builder;
    givenTree(
        root({
            engine("Engine 1", "engine1", {
                group("Sound-related events", "engine1.sound", {
                    eventType("Gunshot", "engine1.sound.gunshot"),
                    eventType("Beep", "engine1.sound.beep"),
                    group("Face detection", "faceDetection", {
                        eventType("Human face", "faceDetection.human"),
                        eventType("Alien face", "engine1.alien")
                    })
                }),
                eventType("Line Crossing", "lineCrossing"),
                eventType("Object in the area", "engine1.objectInTheArea")
            }),
            engine("Engine 2", "engine2", {
                group("Sound-related events", "engine2.sound", {
                    eventType("Gunshot", "engine2.sound.gunshot"),
                    eventType("Beep", "engine2.sound.beep"),
                    group("Face detection", "faceDetection", {
                        eventType("Human face", "faceDetection.human"),
                        group("Alien Face detection", "alienFaceDetection", {
                            eventType("Alien face", "engine2.alien")
                        })
                    }),
                    eventType("Line Crossing", "lineCrossing")
                }),
                eventType("Object in the area", "engine2.objectInTheArea")
            })
        })
    );

    QSet<Node::EntityTypeId> allowedEntites;
    allowedEntites.insert("faceDetection.human");
    allowedEntites.insert("alienFaceDetection");
    allowedEntites.insert("lineCrossing");

    whenFilterTreeInclusive(
        [allowedEntites](NodePtr node) { return allowedEntites.contains(node->entityId); });

    expectTree(
        root({
            engine("Engine 1", "engine1", {
                group("Sound-related events", "engine1.sound", {
                    group("Face detection", "faceDetection", {
                        eventType("Human face", "faceDetection.human")
                    })
                }),
                eventType("Line Crossing", "lineCrossing")
            }),
            engine("Engine 2", "engine2", {
                group("Sound-related events", "engine2.sound", {
                    group("Face detection", "faceDetection", {
                        eventType("Human face", "faceDetection.human"),
                        group("Alien Face detection", "alienFaceDetection", {
                            eventType("Alien face", "engine2.alien")
                        })
                    }),
                    eventType("Line Crossing", "lineCrossing")
                })
            })
        })
    );
}

} // namespace test
} // namespace nx::vms::client::desktop
