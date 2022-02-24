// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/node_view/details/node_view_state.h>
#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_data.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_helper.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_data_builder.h>
#include <nx/utils/scope_guard.h>

using namespace nx::vms::client::desktop::node_view;
using namespace nx::vms::client::desktop::node_view::details;

namespace {

static constexpr int kDefaultTextColumn = 0;
static const auto kNodeText = QString("Test node text");
static const auto kAnotherNodeText = QString("Some text");

NodePtr createTestTree()
{
    return ViewNodeHelper::createSimpleNode("root", {
        ViewNodeHelper::createSimpleNode("1",
            { ViewNodeHelper::createSimpleNode("1_1")}),
        ViewNodeHelper::createSimpleNode("2",
            { ViewNodeHelper::createSimpleNode("2_1"),
              ViewNodeHelper::createSimpleNode("2_2")})
        });
}

NodeViewStatePatch::GetNodeOperationGuard createCheckAllPathsGuard(
    PatchStepList& steps,
    PatchStepOperation operation)
{
    return
        [&steps, operation](const PatchStep& step)
        {
            return nx::utils::makeSharedGuard(
                [step, operation, &steps]()
                {
                    ASSERT_TRUE(step.operationData.operation == operation);
                    const auto it = std::find_if(steps.begin(), steps.end(),
                        [path = step.path](const PatchStep& patchStep)
                        {
                            return patchStep.path == path;
                        });
                    ASSERT_FALSE(it == steps.end());
                    steps.erase(it);
                });
        };
}

void recursiveCall(const NodePtr& node, std::function<void (const NodePtr& )> functor)
{
    if (!functor)
        return;

    for (const auto child: node->children())
        recursiveCall(child, functor);

    functor(node);
}

} // namespace

TEST(NodeViewStatePatchTest, patch_from_node)
{
    const auto rootNode = createTestTree();
    const auto patch = NodeViewStatePatch::fromRootNode(rootNode);

    NodeViewState state;
    state = patch.applyTo(std::move(state));

    ASSERT_FALSE(state.rootNode.isNull());
    ASSERT_TRUE(state.rootNode->childrenCount() == 2);

    const auto child1 = state.rootNode->nodeAt(0);
    ASSERT_TRUE(ViewNodeHelper(child1).text(kDefaultTextColumn) == "1");
    ASSERT_TRUE(child1->childrenCount() == 1);
    ASSERT_TRUE(ViewNodeHelper(child1->nodeAt(0)).text(kDefaultTextColumn) == "1_1");

    const auto child2 = state.rootNode->nodeAt(1);
    ASSERT_TRUE(ViewNodeHelper(child2).text(kDefaultTextColumn) == "2");
    ASSERT_TRUE(child2->childrenCount() == 2);
    ASSERT_TRUE(ViewNodeHelper(child2->nodeAt(0)).text(kDefaultTextColumn) == "2_1");
    ASSERT_TRUE(ViewNodeHelper(child2->nodeAt(1)).text(kDefaultTextColumn) == "2_2");
}

TEST(NodeViewStatePatchTest, simple_add_node)
{
    NodeViewStatePatch patch;

    patch.addAppendStep(ViewNodePath(), ViewNodeDataBuilder().separator());

    ASSERT_TRUE(patch.steps.size() == 1);
    ASSERT_TRUE(patch.steps.front().path == ViewNodePath());
    ASSERT_TRUE(patch.steps.front().operationData.operation == appendNodeOperation);

    NodeViewState state;
    state = patch.applyTo(std::move(state));

    ASSERT_FALSE(state.rootNode.isNull());
    ASSERT_TRUE(ViewNodeHelper(state.rootNode->nodeAt(0)).isSeparator());
}

TEST(NodeViewStatePatchTest, change_node)
{
    NodeViewStatePatch patch;
    patch.addUpdateDataStep(ViewNodePath(),
        ViewNodeDataBuilder().withText(kDefaultTextColumn, kAnotherNodeText));

    ASSERT_TRUE(patch.steps.size() == 1);
    ASSERT_TRUE(patch.steps.front().path == ViewNodePath());
    ASSERT_TRUE(patch.steps.front().operationData.operation == updateDataOperation);

    NodeViewState state;
    state.rootNode = ViewNodeHelper::createSimpleNode(kNodeText);
    state = patch.applyTo(std::move(state));

    ASSERT_FALSE(state.rootNode.isNull());
    ASSERT_TRUE(ViewNodeHelper(state.rootNode).text(kDefaultTextColumn) == kAnotherNodeText);
}

TEST(NodeViewStatePatchTest, simple_remove_node)
{
    NodeViewStatePatch patch;
    patch.addRemovalStep(ViewNodePath());

    ASSERT_TRUE(patch.steps.size() == 1);
    ASSERT_TRUE(patch.steps.front().path == ViewNodePath());
    ASSERT_TRUE(patch.steps.front().operationData.operation == removeNodeOperation);

    NodeViewState state;
    state.rootNode = ViewNodeHelper::createSimpleNode(kNodeText);
    state = patch.applyTo(std::move(state));

    ASSERT_TRUE(state.rootNode.isNull());
}

TEST(NodeViewStatePatchTest, remove_tree_nodes)
{
    const auto treeNodePatch = NodeViewStatePatch::fromRootNode(createTestTree());
    NodeViewState state;
    state = treeNodePatch.applyTo(std::move(state));

    NodeViewStatePatch removeLeafPatch;
    removeLeafPatch.addRemovalStep(ViewNodePath({1, 1}));
    state = removeLeafPatch.applyTo(std::move(state));

    ASSERT_TRUE(state.rootNode->childrenCount() == 2);
    ASSERT_TRUE(state.rootNode->nodeAt(1)->childrenCount() == 1);
    ASSERT_TRUE(ViewNodeHelper(state.rootNode->nodeAt(1)->nodeAt(0)).text(kDefaultTextColumn) == "2_1");

    NodeViewStatePatch removeParentPatch;
    removeParentPatch.addRemovalStep(ViewNodePath({0}));
    state = removeParentPatch.applyTo(std::move(state));

    ASSERT_TRUE(state.rootNode->childrenCount() == 1);
    ASSERT_TRUE(ViewNodeHelper(state.rootNode->nodeAt(0)).text(kDefaultTextColumn) == "2");

    NodeViewStatePatch removeRootPatch;
    removeRootPatch.addRemovalStep(ViewNodePath());
    state = removeRootPatch.applyTo(std::move(state));

    ASSERT_TRUE(state.rootNode.isNull());
}

TEST(NodeViewStatePatchTest, append_guard)
{
    const auto createTreePatch = NodeViewStatePatch::fromRootNode(createTestTree());
    auto appendNodeSteps = createTreePatch.steps;

    NodeViewState state;
    state = createTreePatch.applyTo(std::move(state),
        createCheckAllPathsGuard(appendNodeSteps, appendNodeOperation));

    ASSERT_TRUE(appendNodeSteps.empty());
}

TEST(NodeViewStatePatchTest, change_guard)
{
    const auto createTreePatch = NodeViewStatePatch::fromRootNode(createTestTree());

    NodeViewState state;
    state = createTreePatch.applyTo(std::move(state));

    NodeViewStatePatch changeTextPatch;

    details::ViewNodeHelper::forEachNode(state.rootNode,
        [&changeTextPatch](const NodePtr& node)
        {
            changeTextPatch.addUpdateDataStep(node->path(),
                ViewNodeDataBuilder().withText(kDefaultTextColumn, kNodeText));
        });

    auto changeSteps = changeTextPatch.steps;
    state = changeTextPatch.applyTo(std::move(state),
        createCheckAllPathsGuard(changeSteps, updateDataOperation));

    ASSERT_TRUE(changeSteps.empty());

    const auto checkText =
        [](const NodePtr& node)
        {
            ASSERT_TRUE(ViewNodeHelper(node).text(kDefaultTextColumn) == kNodeText);
        };

    recursiveCall(state.rootNode, checkText);
}

TEST(NodeViewStatePatchTest, remove_guard)
{
    auto testTree = createTestTree();
    const auto createTreePatch = NodeViewStatePatch::fromRootNode(testTree);

    NodeViewState state;
    state = createTreePatch.applyTo(std::move(state));

    int nodesCount = 0;
    details::ViewNodeHelper::forEachNode(state.rootNode,
        [&nodesCount](const NodePtr& /*node*/) { ++nodesCount; });

    int removeGuardCallCount = 0;
    const auto getRemoveNodeGuard =
        [&testTree, &removeGuardCallCount](const PatchStep& step)
        {
            return nx::utils::makeSharedGuard(
                [&testTree, &removeGuardCallCount, step]()
                {
                    ASSERT_TRUE(step.operationData.operation == removeNodeOperation);
                    testTree->nodeAt(step.path.parentPath())->removeChild(step.path.lastIndex());
                    ++removeGuardCallCount;
                });
        };

    NodeViewStatePatch removeNodesPatch;
    removeNodesPatch.addRemovalStep(ViewNodePath());
    state = removeNodesPatch.applyTo(std::move(state), getRemoveNodeGuard);

    int resultNodesCount = 0;
    details::ViewNodeHelper::forEachNode(state.rootNode,
        [&resultNodesCount](const NodePtr& /*node*/) { ++resultNodesCount; });

    ASSERT_TRUE(nodesCount == removeGuardCallCount + 1); //< No remove guard call for root node
    ASSERT_TRUE(resultNodesCount == 0);
}
