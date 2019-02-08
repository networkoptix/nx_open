#include <gtest/gtest.h>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_data.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_helpers.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_data_builder.h>

using namespace nx::vms::client::desktop::node_view;
using namespace nx::vms::client::desktop::node_view::details;

namespace {

enum TestDataRole
{
    otherRole = Qt::UserRole + 1000,
    testIdRole
};

enum TestColumn
{
    firstTestColumn,
    secondTestColumn,
    thirdTestColumn,
    fourthTestColumn
};

static const auto kTestId = QnUuid::createUuid();
static const auto kAnotherTestId = QnUuid::createUuid();
static const auto kTestText = QString("testText");
static const auto kAnotherText = QString("anotherText");
static constexpr int kTestGroupSortOrder = 101;
static const bool kExpandedTestValue = true;
static const auto kSomeTestFlag = Qt::ItemNeverHasChildren;
static const auto kAnotherTestFlag = Qt::ItemIsUserTristate;
static const auto kInitialTestFlag = Qt::ItemIsDragEnabled;

} // namespace

TEST(ViewNodeTest, generic_data_test)
{
    ViewNodeData data;
    ASSERT_TRUE(data.data(firstTestColumn, testIdRole).isNull());

    data.setData(firstTestColumn, testIdRole, QVariant::fromValue(kTestId));

    // Checks value presence and equality to the set one.
    ASSERT_TRUE(data.hasData(firstTestColumn, testIdRole));
    ASSERT_TRUE(data.data(firstTestColumn, testIdRole).value<QnUuid>() == kTestId);

    // Checks that other data is not set.
    ASSERT_TRUE(data.data(firstTestColumn, otherRole).isNull());
    ASSERT_FALSE(data.hasData(secondTestColumn, testIdRole));
    ASSERT_TRUE(data.data(secondTestColumn, testIdRole).isNull());

    // Checks used columns and roles.
    ASSERT_TRUE(data.usedColumns() == ViewNodeData::Columns({firstTestColumn}));
    ASSERT_TRUE(data.rolesForColumn(firstTestColumn) == ViewNodeData::Roles({testIdRole}));
    ASSERT_TRUE(data.rolesForColumn(secondTestColumn).isEmpty());
    ASSERT_TRUE(data.hasDataForColumn(firstTestColumn));
    ASSERT_FALSE(data.hasDataForColumn(secondTestColumn));

    data.removeData(firstTestColumn, testIdRole);
    // Checks if we cleaned data up.
    ASSERT_FALSE(data.hasData(firstTestColumn, testIdRole));
    ASSERT_FALSE(data.hasDataForColumn(firstTestColumn));

    data.setProperty(testIdRole, QVariant::fromValue(kTestId));

    // Checks property presence and value.
    ASSERT_TRUE(data.hasProperty(testIdRole));
    ASSERT_TRUE(data.property(testIdRole).value<QnUuid>() == kTestId);

    data.removeProperty(testIdRole);
    // Checks if property is cleaned up.
    ASSERT_FALSE(data.hasProperty(testIdRole));
}

TEST(ViewNodeTest, base_data_fields_test)
{
    const auto nodeData = ViewNodeDataBuilder()
        .withText(firstTestColumn, kTestText)
        .withCheckedState(firstTestColumn, Qt::Checked)
        .withCheckedState(ColumnSet({secondTestColumn}), Qt::Unchecked)
        .withCheckedState(thirdTestColumn, OptionalCheckedState())
        .withGroupSortOrder(kTestGroupSortOrder)
        .withExpanded(kExpandedTestValue)
        .withFlag(firstTestColumn, kInitialTestFlag)
        .withFlags(secondTestColumn, kSomeTestFlag | kAnotherTestFlag)
        .data();

    const auto node = ViewNode::create(nodeData);

    ASSERT_TRUE(details::text(node, firstTestColumn) == kTestText);
    ASSERT_TRUE(details::text(nodeData, firstTestColumn) == kTestText);

    ASSERT_TRUE(details::checkable(node, firstTestColumn));
    ASSERT_TRUE(details::checkable(nodeData, firstTestColumn));
    ASSERT_TRUE(details::checkedState(node, firstTestColumn) == Qt::Checked);
    ASSERT_TRUE(details::checkedState(nodeData, firstTestColumn) == Qt::Checked);

    ASSERT_TRUE(details::checkable(node, secondTestColumn));
    ASSERT_TRUE(details::checkable(nodeData, secondTestColumn));
    ASSERT_TRUE(details::checkedState(node, secondTestColumn) == Qt::Unchecked);
    ASSERT_TRUE(details::checkedState(nodeData, secondTestColumn) == Qt::Unchecked);

    ASSERT_FALSE(details::checkable(node, thirdTestColumn));
    ASSERT_FALSE(details::checkable(nodeData, thirdTestColumn));
    ASSERT_FALSE(details::checkable(node, fourthTestColumn));
    ASSERT_FALSE(details::checkable(nodeData, fourthTestColumn));

    ASSERT_TRUE(nodeData.flags(firstTestColumn).testFlag(kInitialTestFlag));
    ASSERT_FALSE(nodeData.flags(firstTestColumn).testFlag(kAnotherTestFlag));

    ASSERT_FALSE(nodeData.flags(secondTestColumn).testFlag(kInitialTestFlag));
    ASSERT_TRUE(nodeData.flags(secondTestColumn).testFlag(kSomeTestFlag));
    ASSERT_TRUE(nodeData.flags(secondTestColumn).testFlag(kAnotherTestFlag));
}

TEST(ViewNodeTest, separator)
{
    const auto separatorNodeData = ViewNodeDataBuilder().separator().data();

    // Checks if data represents separator node
    ASSERT_TRUE(details::isSeparator(separatorNodeData));
    ASSERT_TRUE(details::isSeparator(ViewNode::create(separatorNodeData)));
}

TEST(ViewNodeTest, apply_data_test)
{
    ViewNodeData target = ViewNodeDataBuilder()
        .withData(firstTestColumn, testIdRole, QVariant::fromValue(kAnotherTestId))
        .withCommonData(otherRole, kAnotherText)
        .withFlag(firstTestColumn, kInitialTestFlag);

    const ViewNodeData changedData = ViewNodeDataBuilder()
        .withData(firstTestColumn, testIdRole, QVariant::fromValue(kTestId))
        .withCommonData(otherRole, kTestText)
        .withFlag(firstTestColumn, kSomeTestFlag)
        .withFlags(secondTestColumn, kAnotherTestFlag);

    target.applyData(changedData);

    // Checks if all data is applied correctly after apply operation.
    ASSERT_TRUE(target.hasData(firstTestColumn, testIdRole));
    ASSERT_TRUE(target.data(firstTestColumn, testIdRole).value<QnUuid>() == kTestId);

    ASSERT_TRUE(target.hasProperty(otherRole));
    ASSERT_TRUE(target.property(otherRole).toString() == kTestText);

    ASSERT_FALSE(target.flags(firstTestColumn).testFlag(kInitialTestFlag));

    ASSERT_TRUE(target.flags(firstTestColumn).testFlag(kSomeTestFlag));
    ASSERT_FALSE(target.flags(firstTestColumn).testFlag(kAnotherTestFlag));
    ASSERT_TRUE(target.flags(secondTestColumn).testFlag(kAnotherTestFlag));
    ASSERT_FALSE(target.flags(secondTestColumn).testFlag(kSomeTestFlag));
}

TEST(ViewNodeTest, init)
{
    const auto node = ViewNode::create();
    ASSERT_FALSE(node.isNull());
    ASSERT_TRUE(node->isLeaf());
    ASSERT_TRUE(node->isRoot());
    ASSERT_TRUE(node->children().isEmpty());
    ASSERT_TRUE(node->childrenCount() == 0);
    ASSERT_TRUE(node->parent().isNull());
}

TEST(ViewNodeTest, node_test)
{
    const auto firstChild = ViewNode::create();
    const auto root = ViewNode::create({firstChild});

    ASSERT_TRUE(root->childrenCount() == 1);
    ASSERT_TRUE(root->children().first() == firstChild);
    ASSERT_TRUE(root->indexOf(firstChild) == 0);
    ASSERT_TRUE(firstChild->parent() == root);

    const auto secondChild = ViewNode::create();
    root->addChild(secondChild);

    ASSERT_TRUE(root->childrenCount() == 2);
    ASSERT_TRUE(root->indexOf(firstChild) == 0);
    ASSERT_TRUE(root->indexOf(secondChild) == 1);
    ASSERT_TRUE(root->children().at(0) == firstChild);
    ASSERT_TRUE(root->children().at(1) == secondChild);
    ASSERT_TRUE(root->nodeAt(0) == firstChild);
    ASSERT_TRUE(root->nodeAt(1) == secondChild);

    ASSERT_TRUE(root->path() == ViewNodePath());
    ASSERT_TRUE(firstChild->path() == ViewNodePath({0}));
    ASSERT_TRUE(secondChild->path() == ViewNodePath({1}));

    ASSERT_TRUE(root->nodeAt(firstChild->path()) == firstChild);
    ASSERT_TRUE(root->nodeAt(secondChild->path()) == secondChild);

    root->removeChild(firstChild);
    ASSERT_TRUE(root->childrenCount() == 1);
    ASSERT_TRUE(root->children().first() == secondChild);
    ASSERT_TRUE(root->nodeAt(0) == secondChild);
    ASSERT_TRUE(root->indexOf(secondChild) == 0);

    root->removeChild(0);
    ASSERT_TRUE(root->childrenCount() == 0);

    ASSERT_TRUE(root->indexOf(firstChild) == -1);
    ASSERT_TRUE(root->indexOf(secondChild) == -1);
}

