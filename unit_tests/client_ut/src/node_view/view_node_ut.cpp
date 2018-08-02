
#include <gtest/gtest.h>

#include <nx/utils/uuid.h>
#include <nx/client/desktop/node_view/details/node/view_node.h>
#include <nx/client/desktop/node_view/details/node/view_node_data.h>
#include <nx/client/desktop/node_view/details/node/view_node_helpers.h>
#include <nx/client/desktop/node_view/details/node/view_node_data_builder.h>

using namespace nx::client::desktop::node_view;
using namespace nx::client::desktop::node_view::details;

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
static constexpr int kTestSiblingGroup = 101;
static const bool kExpandedTestValue = true;
static const auto kSomeTestFlag = Qt::ItemNeverHasChildren;
static const auto kAnotherTestFlag = Qt::ItemIsUserTristate;
static const auto kInitialTestFlag = Qt::ItemIsDragEnabled;

} // namespace

TEST(ViewNodeTest, init)
{
    const auto root = ViewNode::create();
    ASSERT_TRUE(root->isLeaf());
    ASSERT_TRUE(root->isRoot());
}

TEST(ViewNodeTest, generic_data_test)
{
    ViewNodeData data;
    ASSERT_TRUE(data.data(firstTestColumn, testIdRole).isNull());

    data.setData(firstTestColumn, testIdRole, QVariant::fromValue(kTestId));

    ASSERT_TRUE(data.hasData(firstTestColumn, testIdRole));
    ASSERT_TRUE(data.data(firstTestColumn, testIdRole).value<QnUuid>() == kTestId);
    ASSERT_TRUE(data.data(firstTestColumn, otherRole).isNull());
    ASSERT_FALSE(data.hasData(secondTestColumn, testIdRole));
    ASSERT_TRUE(data.data(secondTestColumn, testIdRole).isNull());

    ASSERT_TRUE(data.usedColumns() == ViewNodeData::Columns({firstTestColumn}));
    ASSERT_TRUE(data.rolesForColumn(firstTestColumn) == ViewNodeData::Roles({testIdRole}));
    ASSERT_TRUE(data.rolesForColumn(secondTestColumn).isEmpty());

    ASSERT_TRUE(data.hasDataForColumn(firstTestColumn));
    ASSERT_FALSE(data.hasDataForColumn(secondTestColumn));

    data.removeData(firstTestColumn, testIdRole);
    ASSERT_FALSE(data.hasData(firstTestColumn, testIdRole));
    ASSERT_FALSE(data.hasDataForColumn(firstTestColumn));

    data.setCommonNodeData(testIdRole, QVariant::fromValue(kTestId));
    ASSERT_TRUE(data.hasCommonData(testIdRole));

    data.removeCommonNodeData(testIdRole);
    ASSERT_FALSE(data.hasCommonData(testIdRole));
}

TEST(ViewNodeTest, apply_data_test)
{
    ViewNodeData target = ViewNodeDataBuilder()
        .withData(firstTestColumn, testIdRole, QVariant::fromValue(kAnotherTestId))
        .withCommonData(otherRole, kAnotherText)
        .withFlag(firstTestColumn, kInitialTestFlag);

    ViewNodeData changedData = ViewNodeDataBuilder()
        .withData(firstTestColumn, testIdRole, QVariant::fromValue(kTestId))
        .withCommonData(otherRole, kTestText)
        .withFlag(firstTestColumn, kSomeTestFlag)
        .withFlags(secondTestColumn, kAnotherTestFlag);

    target.applyData(changedData);

    ASSERT_TRUE(target.hasData(firstTestColumn, testIdRole));
    ASSERT_TRUE(target.data(firstTestColumn, testIdRole).value<QnUuid>() == kTestId);

    ASSERT_TRUE(target.hasCommonData(otherRole));
    ASSERT_TRUE(target.commonNodeData(otherRole).toString() == kTestText);

    ASSERT_FALSE(target.flags(firstTestColumn).testFlag(kInitialTestFlag));

    ASSERT_TRUE(target.flags(firstTestColumn).testFlag(kSomeTestFlag));
    ASSERT_FALSE(target.flags(firstTestColumn).testFlag(kAnotherTestFlag));
    ASSERT_TRUE(target.flags(secondTestColumn).testFlag(kAnotherTestFlag));
    ASSERT_FALSE(target.flags(secondTestColumn).testFlag(kSomeTestFlag));
}

TEST(ViewNodeTest, base_data_fields_test)
{
    const auto separatorNodeData = ViewNodeDataBuilder().separator().data();
    ASSERT_TRUE(details::isSeparator(separatorNodeData));
    ASSERT_TRUE(details::isSeparator(ViewNode::create(separatorNodeData)));

    const auto nodeData = ViewNodeDataBuilder()
        .withText(firstTestColumn, kTestText)
        .withCheckedState(firstTestColumn, Qt::Checked)
        .withCheckedState(ColumnsSet({secondTestColumn}), Qt::Unchecked)
        .withCheckedState(thirdTestColumn, OptionalCheckedState())
        .withSiblingGroup(kTestSiblingGroup)
        .withExpanded(kExpandedTestValue)
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
}


