#include <gtest/gtest.h>
#include <nx/vms/client/desktop/analytics/analytics_entities_tree.h>

namespace nx::vms::client::desktop {
namespace test {

using Node = AnalyticsEntitiesTreeBuilder::Node;
using NodeType = AnalyticsEntitiesTreeBuilder::NodeType;
using NodePtr = AnalyticsEntitiesTreeBuilder::NodePtr;

TEST(AnalyticsEntitiesTreeBuilder, filterSingleEngine)
{
    NodePtr engine(new Node(NodeType::engine));
    NodePtr group1(new Node(NodeType::group, "group1"));
    engine->children.push_back(group1);
    group1->children.emplace_back(new Node(NodeType::eventType, "event1"));
    group1->children.emplace_back(new Node(NodeType::eventType)); //< To filter out.

    NodePtr group2(new Node(NodeType::group, "group2"));
    engine->children.push_back(group2);
    group2->children.emplace_back(new Node(NodeType::eventType)); //< To filter out.

    engine->children.emplace_back(new Node(NodeType::eventType, "event2"));
    engine->children.emplace_back(new Node(NodeType::eventType)); //< To filter out.

    // Filter out event type nodes with empty text.
    const auto filtered_engine = AnalyticsEntitiesTreeBuilder::filterTree(engine,
        [](NodePtr node) { return node->nodeType == NodeType::eventType && node->text.isEmpty(); });

    // Filtered tree must still have an engine as the root.
    ASSERT_EQ(filtered_engine->nodeType, NodeType::engine);

    // Only group1 and event2 should left. Group2 will be removed as an empty group.
    ASSERT_EQ(filtered_engine->children.size(), 2);

    const auto filtered_group = filtered_engine->children[0];
    ASSERT_EQ(filtered_group->nodeType, NodeType::group);
    ASSERT_EQ(filtered_group->children.size(), 1);

    const auto event1 = filtered_group->children[0];
    ASSERT_EQ(event1->nodeType, NodeType::eventType);
    ASSERT_EQ(event1->text, "event1");

    const auto event2 = filtered_engine->children[1];
    ASSERT_EQ(event2->nodeType, NodeType::eventType);
    ASSERT_EQ(event2->text, "event2");
}

TEST(AnalyticsEntitiesTreeBuilder, filterOutEmptyEngines)
{
    NodePtr root(new Node(NodeType::root));
    NodePtr engine(new Node(NodeType::engine));
    root->children.push_back(engine);
    engine->children.emplace_back(new Node(NodeType::group));
    root->children.emplace_back(new Node(NodeType::engine));

    // Do not filter something out. Just collapse the tree.
    const auto filtered_root = AnalyticsEntitiesTreeBuilder::filterTree(root);

    ASSERT_EQ(filtered_root->nodeType, NodeType::root);
    ASSERT_TRUE(filtered_root->children.empty());
}

TEST(AnalyticsEntitiesTreeBuilder, pullOutSingleEngine)
{
    NodePtr root(new Node(NodeType::root));
    NodePtr engine(new Node(NodeType::engine));
    root->children.push_back(engine);
    engine->children.emplace_back(new Node(NodeType::eventType));

    // Do not filter something out. Just collapse the tree.
    const auto filtered_root = AnalyticsEntitiesTreeBuilder::filterTree(root);

    ASSERT_EQ(filtered_root->nodeType, NodeType::engine);
}


} // namespace test
} // namespace nx::vms::client::desktop
