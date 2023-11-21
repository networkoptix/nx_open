// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <iostream>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QStringList>
#include <QtGui/QStandardItemModel>
#include <QtTest/QAbstractItemModelTester>

#include <QtCore/private/qabstractitemmodel_p.h>

#include <nx/vms/client/desktop/common/models/linearization_list_model.h>

#include "test_item_model.h"

namespace nx::vms::client::desktop {
namespace test {

// Storage to track dataChanged signals history.
struct DataChange
{
    QVector<int> roles;
    QString top, bottom;

    DataChange(const QVector<int>& roles, const QString& top, const QString& bottom = ""):
        roles(roles),
        top(top),
        bottom(bottom.isEmpty() ? top : bottom)
    {
        std::sort(this->roles.begin(), this->roles.end());
    }

    bool operator==(const DataChange& other) const
    {
        return roles == other.roles && top == other.top && bottom == other.bottom;
    }
};

std::ostream& operator<<(std::ostream& s, const DataChange& dataChange)
{
    s << "{{";
    if (!dataChange.roles.empty())
    {
        auto iter = dataChange.roles.cbegin();
        s << (*iter);
        for (++iter; iter != dataChange.roles.cend(); ++iter)
            s << ", " << (*iter);
    }
    s << "}, " << dataChange.top.toStdString() << "..." << dataChange.bottom.toStdString() << "}";
    return s;
}

class LinearizationListModelTest: public testing::Test
{
public:
    virtual void SetUp() override
    {
        sourceModel.reset(new TestItemModel());
        sourceModel->appendRow(new QStandardItem("0"));
        sourceModel->item(0)->appendRow(new QStandardItem("00"));
        sourceModel->item(0)->appendRow(new QStandardItem("01"));
        sourceModel->appendRow(new QStandardItem("1"));
        sourceModel->item(1)->appendRow(new QStandardItem("10"));
        sourceModel->item(1)->appendRow(new QStandardItem("11"));
        sourceModel->item(1)->child(1)->appendRow(new QStandardItem("110"));
        sourceModel->item(1)->child(1)->appendRow(new QStandardItem("111"));
        sourceModel->item(1)->child(1)->appendRow(new QStandardItem("112"));
        sourceModel->item(1)->appendRow(new QStandardItem("12"));
        sourceModel->appendRow(new QStandardItem("2"));
        sourceModel->appendRow(new QStandardItem("3"));
        sourceModel->item(3)->appendRow(new QStandardItem("30"));

        testModel.reset(new LinearizationListModel());
        testModel->setSourceModel(sourceModel.get());

        modelTester.reset(new QAbstractItemModelTester(testModel.get(),
            QAbstractItemModelTester::FailureReportingMode::Fatal));

        QObject::connect(testModel.get(), &QAbstractItemModel::dataChanged,
            [this](const QModelIndex& top, const QModelIndex& bottom, const QVector<int>& roles)
            {
                if (dataRoleFilter == -1)
                    dataChanges << DataChange({roles, toString(top), toString(bottom)});
                else if (roles.empty() || roles.contains(dataRoleFilter))
                    dataChanges << DataChange({{dataRoleFilter}, toString(top), toString(bottom)});
            });

        QObject::connect(sourceModel.get(), &QAbstractItemModel::dataChanged,
            [this](const QModelIndex& top, const QModelIndex& bottom, const QVector<int>& roles)
            {
                if (dataRoleFilter == -1)
                    sourceDataChanges << DataChange({roles, toString(top), toString(bottom)});
                else if (roles.empty() || roles.contains(dataRoleFilter))
                    sourceDataChanges << DataChange({{dataRoleFilter}, toString(top), toString(bottom)});
            });

        QObject::connect(testModel.get(), &QAbstractItemModel::rowsAboutToBeInserted,
            [this](const QModelIndex& parent, int first, int last)
            {
                previousRowCount = testModel->rowCount();
                ASSERT_FALSE(parent.isValid());
                ASSERT_TRUE(last >= first);
                ASSERT_TRUE(first >= 0 && first <= previousRowCount);
            });

        QObject::connect(testModel.get(), &QAbstractItemModel::rowsInserted,
            [this](const QModelIndex& /*parent*/, int first, int last)
            {
                ASSERT_EQ(previousRowCount + (last - first + 1), testModel->rowCount());
            });

        QObject::connect(testModel.get(), &QAbstractItemModel::rowsAboutToBeRemoved,
            [this](const QModelIndex& parent, int first, int last)
            {
                previousRowCount = testModel->rowCount();
                ASSERT_FALSE(parent.isValid());
                ASSERT_TRUE(last >= first);
                ASSERT_TRUE(first >= 0 && last < previousRowCount);
            });

        QObject::connect(testModel.get(), &QAbstractItemModel::rowsRemoved,
            [this](const QModelIndex& /*parent*/, int first, int last)
            {
                ASSERT_EQ(previousRowCount - (last - first + 1), testModel->rowCount());
            });

        dataRoleFilter = -1;
        sourceDataChanges.clear();
        dataChanges.clear();
    }

    virtual void TearDown() override
    {
        modelTester.reset();
        testModel.reset();
        sourceModel.reset();
    }

    template<class T>
    QList<T> itemData(int role) const
    {
        QList<T> result;
        for (int row = 0, count = testModel->rowCount(); row < count; ++row)
            result += testModel->data(testModel->index(row), role).value<T>();

        return result;
    }

    QModelIndex sourceIndex(std::initializer_list<int> indices) const
    {
        return sourceModel->buildIndex(indices);
    }

    using LLM = LinearizationListModel;

    std::unique_ptr<TestItemModel> sourceModel;
    std::unique_ptr<LinearizationListModel> testModel;
    std::unique_ptr<QAbstractItemModelTester> modelTester;

    QList<DataChange> sourceDataChanges;
    QList<DataChange> dataChanges;
    int dataRoleFilter = -1;

    int previousRowCount = 0;
};

TEST_F(LinearizationListModelTest, testItemModelInternalTest)
{
    const QPersistentModelIndex testIndex1(sourceIndex({1}));
    const QPersistentModelIndex testIndex2(sourceIndex({1, 1}));
    const QPersistentModelIndex testIndex3(sourceIndex({1, 1, 1}));
    const QPersistentModelIndex testIndex4(sourceIndex({1, 2}));
    const QPersistentModelIndex testIndex5(sourceIndex({2}));
    const QPersistentModelIndex testIndex6(sourceIndex({3, 0}));

    // Invalid move attempts.
    ASSERT_FALSE(sourceModel->moveRows(sourceIndex({1}), 0, 2, sourceIndex({1}), 0));
    ASSERT_FALSE(sourceModel->moveRows(sourceIndex({1}), 0, 2, sourceIndex({1}), 1));
    ASSERT_FALSE(sourceModel->moveRows(sourceIndex({1}), 0, 2, sourceIndex({1}), 2));
    ASSERT_FALSE(sourceModel->moveRows(sourceIndex({1}), 1, 1, sourceIndex({1, 1}), 0));

    // A valid move.
    ASSERT_TRUE(sourceModel->moveRow(sourceIndex({1}), 1, {}, 1));

    ASSERT_EQ(testIndex1, sourceIndex({2}));
    ASSERT_EQ(testIndex2, sourceIndex({1}));
    ASSERT_EQ(testIndex3, sourceIndex({1, 1}));
    ASSERT_EQ(testIndex4, sourceIndex({2, 1}));
    ASSERT_EQ(testIndex5, sourceIndex({3}));
    ASSERT_EQ(testIndex6, sourceIndex({4, 0}));

    const auto text =
        [](const QModelIndex& index) { return index.data(Qt::DisplayRole).toString(); };

    ASSERT_EQ(text(testIndex1), "1");
    ASSERT_EQ(text(testIndex2), "11");
    ASSERT_EQ(text(testIndex3), "111");
    ASSERT_EQ(text(testIndex4), "12");
    ASSERT_EQ(text(testIndex5), "2");
    ASSERT_EQ(text(testIndex6), "30");
}

TEST_F(LinearizationListModelTest, initialState)
{
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "1", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0, 0, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>({true, true, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({false, false, false, false}));
}

TEST_F(LinearizationListModelTest, customSourceRoot)
{
    testModel->setSourceRoot(sourceIndex({1}));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"10", "11", "12"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>({false, true, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({false, false, false}));
}

TEST_F(LinearizationListModelTest, mapping)
{
    ASSERT_EQ(testModel->mapToSource({}), QModelIndex());
    ASSERT_EQ(testModel->mapFromSource({}), QModelIndex());
    ASSERT_EQ(testModel->mapToSource(testModel->index(0)), sourceIndex({0}));
    ASSERT_EQ(testModel->mapFromSource(sourceIndex({0})), testModel->index(0));
    ASSERT_EQ(testModel->mapFromSource(sourceIndex({0, 0})), QModelIndex());

    ASSERT_TRUE(testModel->setData(testModel->index(1), true, LLM::ExpandedRole));
    ASSERT_EQ(testModel->mapToSource(testModel->index(2)), sourceIndex({1, 0}));
    ASSERT_EQ(testModel->mapFromSource(sourceIndex({1, 0})), testModel->index(2));

    ASSERT_TRUE(testModel->setData(testModel->index(3), true, LLM::ExpandedRole));
    ASSERT_EQ(testModel->mapToSource(testModel->index(4)), sourceIndex({1, 1, 0}));
    ASSERT_EQ(testModel->mapFromSource(sourceIndex({1, 1, 0})), testModel->index(4));
    ASSERT_EQ(testModel->mapToSource(testModel->index(7)), sourceIndex({1, 2}));
    ASSERT_EQ(testModel->mapFromSource(sourceIndex({1, 2})), testModel->index(7));
    ASSERT_EQ(testModel->mapToSource(testModel->index(8)), sourceIndex({2}));
    ASSERT_EQ(testModel->mapFromSource(sourceIndex({2})), testModel->index(8));

    testModel->setSourceRoot(sourceIndex({1}));
    ASSERT_EQ(testModel->mapToSource({}), sourceIndex({1}));
    ASSERT_EQ(testModel->mapFromSource({}), QModelIndex());
    ASSERT_EQ(testModel->mapFromSource(sourceIndex({1})), QModelIndex());
    ASSERT_EQ(testModel->mapToSource(testModel->index(0)), sourceIndex({1, 0}));
    ASSERT_EQ(testModel->mapFromSource(sourceIndex({0})), QModelIndex());
    ASSERT_EQ(testModel->mapFromSource(sourceIndex({1, 1})), testModel->index(1));

    ASSERT_TRUE(testModel->setData(testModel->index(1), true, LLM::ExpandedRole));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole),
        QStringList({"10", "11", "110", "111", "112", "12"}));
    ASSERT_EQ(testModel->mapToSource(testModel->index(2)), sourceIndex({1, 1, 0}));
    ASSERT_EQ(testModel->mapFromSource(sourceIndex({1, 1, 2})), testModel->index(4));
}

TEST_F(LinearizationListModelTest, expandCollapse)
{
    // Can't collapse an item already collapsed.
    ASSERT_FALSE(testModel->setData(testModel->index(0), false, LLM::ExpandedRole));

    // Expand the "0" item.
    ASSERT_TRUE(testModel->setData(testModel->index(0), true, LLM::ExpandedRole));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "00", "01", "1", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0, 1, 1, 0, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, false, false, false}));

    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::ExpandedRole}, "0"}}));

    // Can't expand an item already expanded.
    ASSERT_FALSE(testModel->setData(testModel->index(0), true, LLM::ExpandedRole));

    // Can't expand or collapse an item without children.
    ASSERT_FALSE(testModel->setData(testModel->index(2), true, LLM::ExpandedRole));
    ASSERT_FALSE(testModel->setData(testModel->index(2), false, LLM::ExpandedRole));

    // Expand all items recursively.
    testModel->expandAll();
    dataChanges.clear(); //< We don't care at what order expandAll expands items.

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "10", "11", "110", "111", "112", "12", "2", "3", "30"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 2, 2, 2, 1, 0, 0, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true, false, false, false, false, false, true, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, true, false, false, false, false, false, true, false}));

    // Collapse the "1" branch.
    ASSERT_TRUE(testModel->setData(testModel->index(3), false, LLM::ExpandedRole));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "2", "3", "30"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 0, 0, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, false, false, true, false}));

    // Expand the "1" branch and check that expanded children got re-expanded.
    ASSERT_TRUE(testModel->setData(testModel->index(3), true, LLM::ExpandedRole));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "10", "11", "110", "111", "112", "12", "2", "3", "30"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 2, 2, 2, 1, 0, 0, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true, false, false, false, false, false, true, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, true, false, false, false, false, false, true, false}));

    // Collapse the "11" branch.
    ASSERT_TRUE(testModel->setData(testModel->index(5), false, LLM::ExpandedRole));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "10", "11", "12", "2", "3", "30"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 1, 0, 0, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true, false, false, true, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, false, false, false, true, false}));

    // Collapse and expand the "1" branch.
    ASSERT_TRUE(testModel->setData(testModel->index(3), false, LLM::ExpandedRole));
    ASSERT_TRUE(testModel->setData(testModel->index(3), true, LLM::ExpandedRole));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "10", "11", "12", "2", "3", "30"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 1, 0, 0, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true, false, false, true, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, false, false, false, true, false}));

    // Expand the "11" branch.
    ASSERT_TRUE(testModel->setData(testModel->index(5), true, LLM::ExpandedRole));

    ASSERT_EQ(dataChanges, QList<DataChange>({
        {{LLM::ExpandedRole}, "3"},
        {{LLM::ExpandedRole}, "3"},
        {{LLM::ExpandedRole}, "5"},
        {{LLM::ExpandedRole}, "3"},
        {{LLM::ExpandedRole}, "3"},
        {{LLM::ExpandedRole}, "5"}}));

    // Collapse all items recursively.
    testModel->collapseAll();
    dataChanges.clear(); //< We don't care at what order collapseAll collapses items.

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "1", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0, 0, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>({true, true, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({false, false, false, false}));

    // Expand the "1" branch.
    ASSERT_TRUE(testModel->setData(testModel->index(1), true, LLM::ExpandedRole));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "12", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 0, 1, 1, 1, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, true, false, true, false, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {false, true, false, false, false, false, false}));

    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::ExpandedRole}, "1"}}));
}

TEST_F(LinearizationListModelTest, ensureVisible)
{
    // Does nothing.
    testModel->ensureVisible({QModelIndex(), sourceIndex({1})});

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "1", "2", "3"}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({false, false, false, false}));

    // Expands {1}, {1, 1} and {3}.
    testModel->ensureVisible({
        sourceIndex({1, 1, 0}),
        sourceIndex({1, 1, 2}),
        sourceIndex({3, 0})});

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "110", "111", "112", "12", "2", "3", "30"}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {false, true, false, true, false, false, false, false, false, true, false}));
}

TEST_F(LinearizationListModelTest, setData)
{
    dataRoleFilter = Qt::DisplayRole;

    // Can't set LevelRole or HasChildrenRole.
    ASSERT_FALSE(testModel->setData(testModel->index(0), 100500, LLM::LevelRole));
    ASSERT_FALSE(testModel->setData(testModel->index(0), false, LLM::HasChildrenRole));

    // Perform actual changes.
    ASSERT_TRUE(sourceModel->setData(sourceIndex({2}), "-2-", Qt::DisplayRole));
    ASSERT_TRUE(sourceModel->setData(sourceIndex({1, 1, 0}), "-110-", Qt::DisplayRole));

    testModel->expandAll();

    ASSERT_TRUE(testModel->setData(testModel->index(0), "*0*", Qt::DisplayRole));
    ASSERT_TRUE(testModel->setData(testModel->index(7), "*111*", Qt::DisplayRole));
    ASSERT_TRUE(sourceModel->setData(sourceIndex({1, 1, 2}), "-112-", Qt::DisplayRole));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"*0*", "00", "01", "1", "10", "11", "-110-", "*111*", "-112-", "12", "-2-", "3", "30"}));

    ASSERT_EQ(dataChanges, QList<DataChange>({
        {{Qt::DisplayRole}, "2"},
        {{Qt::DisplayRole}, "0"},
        {{Qt::DisplayRole}, "7"},
        {{Qt::DisplayRole}, "8"}}));

    ASSERT_EQ(sourceDataChanges, QList<DataChange>({
        {{Qt::DisplayRole}, "2"},
        {{Qt::DisplayRole}, "1:1:0"},
        {{Qt::DisplayRole}, "0"},
        {{Qt::DisplayRole}, "1:1:1"},
        {{Qt::DisplayRole}, "1:1:2"}}));
}

TEST_F(LinearizationListModelTest, sourceRowsRemoved)
{
    // Expand all except "3".
    testModel->expandAll();
    ASSERT_TRUE(testModel->setData(testModel->index(11), false, LLM::ExpandedRole));
    dataChanges.clear();

    // Remove "30" while "3" is collapsed.
    sourceModel->removeRow(0, sourceIndex({3}));

    // Remove "111", "112", "113".
    sourceModel->removeRows(0, 3, sourceIndex({1, 1}));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "10", "11", "12", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 1, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, false, false, false, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, false, false, false, false}));

    // Remove "1" and "2".
    sourceModel->removeRows(1, 2);

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "00", "01", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({ 0, 1, 1, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>({true, false, false, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({true, false, false, false}));

    // Remove "0".
    sourceModel->removeRow(0);

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>({false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({false}));

    ASSERT_EQ(dataChanges, QList<DataChange>({
        {{LLM::HasChildrenRole}, "11"},
        {{LLM::HasChildrenRole, LLM::ExpandedRole}, "5"}}));
}

TEST_F(LinearizationListModelTest, sourceRootRowRemoved)
{
    testModel->setSourceRoot(sourceIndex({1, 1}));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"110", "111", "112"}));

    sourceModel->removeRows(1, 2);
    ASSERT_EQ(testModel->sourceRoot(), QModelIndex());
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "3"}));
}

TEST_F(LinearizationListModelTest, sourceRowsInserted)
{
    sourceModel->insertRow(1, new QStandardItem("x"));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "x", "1", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0, 0, 0, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>({true, false, true, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({false, false, false, false, false}));

    sourceModel->item(1)->insertRows(0, {new QStandardItem("x0"), new QStandardItem("x1")});
    sourceModel->item(2)->child(1)->insertRow(3, new QStandardItem("113"));
    sourceModel->item(4)->child(0)->appendRow(new QStandardItem("300"));

    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::HasChildrenRole}, "1"}}));

    testModel->expandAll();
    dataChanges.clear();

    sourceModel->item(2)->child(1)->insertRow(4, new QStandardItem("114"));
    sourceModel->item(3)->insertRow(0, new QStandardItem("20"));
    sourceModel->item(4)->insertRow(1, new QStandardItem("31"));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "x", "x0", "x1", "1", "10", "11",
            "110", "111", "112", "113", "114", "12", "2", "3", "30", "300", "31"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 0, 1, 1, 2, 2, 2, 2, 2, 1, 0, 0, 1, 2, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, false, true, false, true,
            false, false, false, false, false, false, true, true, true, false, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, false, true, false, true,
            false, false, false, false, false, false, false, true, true, false, false }));

    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::HasChildrenRole}, "15"}}));
}
TEST_F(LinearizationListModelTest, sourceRootRowMoved)
{
    testModel->setSourceRoot(sourceIndex({1, 1}));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"110", "111", "112"}));

    ASSERT_TRUE(sourceModel->moveRow(sourceIndex({1}), 1, {}, 0));
    ASSERT_EQ(testModel->sourceRoot(), sourceIndex({0}));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"110", "111", "112"}));
}

TEST_F(LinearizationListModelTest, sourceRowsMoved)
{
    ASSERT_TRUE(testModel->setData(testModel->index(1), true, LLM::ExpandedRole));
    ASSERT_TRUE(testModel->setData(testModel->index(3), true, LLM::ExpandedRole));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "110", "111", "112", "12", "2", "3"}));
    dataChanges.clear();

    // Moving rows between different parents.

    // Moving rows the way it doesn't change linearized rows order, only changes their level.
    ASSERT_TRUE(sourceModel->moveRows({}, 2, 1, sourceIndex({1}), 3));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "110", "111", "112", "12", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0, 0, 1, 1, 2, 2, 2, 1, 1, 0}));
    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::LevelRole}, "8", "8"}}));
    dataChanges.clear();

    // ... and back, also without reorder.
    ASSERT_TRUE(sourceModel->moveRows(sourceIndex({1}), 3, 1, {}, 2));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "110", "111", "112", "12", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0, 0, 1, 1, 2, 2, 2, 1, 0, 0}));
    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::LevelRole}, "8", "8"}}));
    dataChanges.clear();

    // Move visible rows to visible expanded parent.
    // This move shifts source parent.
    ASSERT_TRUE(sourceModel->moveRows(sourceIndex({1, 1}), 0, 2, sourceIndex({1}), 1));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "110", "111", "11", "112", "12", "2", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::LevelRole}, "3", "4"}}));
    dataChanges.clear();

    // Move visible row to visible expanded parent.
    // This move causes the source parent to lose all its children.
    ASSERT_TRUE(sourceModel->moveRows(sourceIndex({1, 3}), 0, 1, sourceIndex({1}), 5));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "110", "111", "11", "12", "112", "2", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({
        {{LLM::ExpandedRole, LLM::HasChildrenRole}, "5"},
        {{LLM::LevelRole}, "7"}}));
    dataChanges.clear();

    // Move visible row to visible expanded parent.
    // This move shifts destination parent.
    ASSERT_TRUE(sourceModel->moveRows({}, 0, 1, sourceIndex({1}), 0));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"1", "0", "10", "110", "111", "11", "12", "112", "2", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::LevelRole}, "1"}}));
    dataChanges.clear();

    // Move invisible row to visible collapsed parent: won't expand.
    // This move causes a node to gain children but doesn't change visible rows.
    ASSERT_TRUE(sourceModel->moveRows(sourceIndex({0, 0}), 1, 1, sourceIndex({1}), 0));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"1", "0", "10", "110", "111", "11", "12", "112", "2", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::HasChildrenRole}, "8"}}));
    dataChanges.clear();

    // Move visible rows to visible collapsed parent: will expand.
    ASSERT_TRUE(sourceModel->moveRows(sourceIndex({0}), 2, 2, sourceIndex({1}), 1));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"1", "0", "10", "11", "12", "112", "2", "01", "110", "111", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({
        {{LLM::ExpandedRole}, "8"},
        {{LLM::LevelRole}, "8", "9"}}));
    dataChanges.clear();

    // Move visible row to invisible parent.
    // This move causes node removal.
    ASSERT_TRUE(sourceModel->moveRows(sourceIndex({0}), 4, 1, sourceIndex({0, 0, 0}), 0));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"1", "0", "10", "11", "12", "2", "01", "110", "111", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({}));

    // Move invisible row to visible expanded parent.
    // This move causes node insertion. Source node loses its children.
    ASSERT_TRUE(sourceModel->moveRows(sourceIndex({2}), 0, 1, sourceIndex({1}), 3));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"1", "0", "10", "11", "12", "2", "01", "110", "111", "30", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::HasChildrenRole}, "10"}}));
    dataChanges.clear();

    // Moving rows within the same parent.

    // Moving rows forth.
    ASSERT_TRUE(sourceModel->moveRows(sourceIndex({1}), 1, 2, sourceIndex({1}), 4));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"1", "0", "10", "11", "12", "2", "01", "30", "110", "111", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({}));

    // Moving rows back.
    ASSERT_TRUE(sourceModel->moveRows({}, 1, 2, {}, 0));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"2", "01", "30", "110", "111", "3", "1", "0", "10", "11", "12"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({}));

    // A final full model check.
    testModel->expandAll();
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"2", "01", "30", "110", "111", "3", "1", "0", "00", "112", "10", "11", "12"}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, false, false, false, true, true, true, false, false, false, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, false, false, false, true, true, true, false, false, false, false}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 1, 1, 0, 0, 1, 2, 3, 1, 1, 1}));
}

TEST_F(LinearizationListModelTest, sourceLayoutChanged_verticalSort)
{
    QScopedPointer<QSortFilterProxyModel> sortModel(new QSortFilterProxyModel());
    sortModel->setSourceModel(sourceModel.get());
    testModel->setSourceModel(sortModel.get());

    testModel->expandAll();

    // Initial state:
    // "0", "00", "01", "1", "10", "11", "110", "111", "112", "12", "2", "3", "30"

    QPersistentModelIndex index0(testModel->index(0)); // "0"
    QPersistentModelIndex index2(testModel->index(10)); // "2"
    QPersistentModelIndex index111(testModel->index(7)); // "111"
    QPersistentModelIndex index30(testModel->index(12)); // "30"

    sortModel->setSortRole(Qt::DisplayRole);
    sortModel->sort(0, Qt::DescendingOrder);

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"3", "30", "2", "1", "12", "11", "112", "111", "110", "10", "0", "01", "00"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 0, 0, 1, 1, 2, 2, 2, 1, 0, 1, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true, false, false, false, false, true, false, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, true, false, false, false, false, true, false, false}));

    ASSERT_EQ(index0.row(), 10);
    ASSERT_EQ(index2.row(), 2);
    ASSERT_EQ(index111.row(), 7);
    ASSERT_EQ(index30.row(), 1);

    sortModel->setDynamicSortFilter(true);

    sourceModel->item(1)->child(1)->appendRow(new QStandardItem("113"));
    sourceModel->appendRow(new QStandardItem("2a"));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"3", "30", "2a", "2", "1", "12", "11", "113", "112", "111", "110", "10", "0", "01", "00"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 0, 0, 0, 1, 1, 2, 2, 2, 2, 1, 0, 1, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>({true, false, false, false,
        true, false, true, false, false, false, false, false, true, false, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({true, false, false, false,
        true, false, true, false, false, false, false, false, true, false, false}));

    ASSERT_EQ(index0.row(), 12);
    ASSERT_EQ(index2.row(), 3);
    ASSERT_EQ(index111.row(), 9);
    ASSERT_EQ(index30.row(), 1);

    // Collapse the "1" branch.
    ASSERT_TRUE(testModel->setData(testModel->index(4), false, LLM::ExpandedRole));

    sortModel->sort(0, Qt::AscendingOrder);

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "2", "2a", "3", "30"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 0, 0, 0, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, false, true, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, false, false, false, true, false}));

    ASSERT_EQ(index0.row(), 0);
    ASSERT_EQ(index2.row(), 4);
    ASSERT_FALSE(index111.isValid());
    ASSERT_EQ(index30.row(), 7);
}

TEST_F(LinearizationListModelTest, sourceLayoutChanged_rowsMoving)
{
    // QSortFilterProxyModel transforms rows moving into layout changes.

    QScopedPointer<QSortFilterProxyModel> sortModel(new QSortFilterProxyModel());
    sortModel->setSourceModel(sourceModel.get());
    testModel->setSourceModel(sortModel.get());
    testModel->expandAll();
    testModel->setData(testModel->index(0), false, LLM::ExpandedRole);
    dataChanges.clear();

    // Initial state:
    // "0", "1", "10", "11", "110", "111", "112", "12", "2", "3", "30"

    // Check initial state with the proxy, just in case.
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "110", "111", "112", "12", "2", "3", "30"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 0, 1, 1, 2, 2, 2, 1, 0, 0, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, true, false, true, false, false, false, false, false, true, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {false, true, false, true, false, false, false, false, false, true, false}));

    const QPersistentModelIndex testIndex1(testModel->index(10)); // "30", being moved.
    const QPersistentModelIndex testIndex2(testModel->index(3));
    const QPersistentModelIndex testIndex3(testModel->index(9));

    // Move visible row to visible expanded parent ("30" into "11").
    ASSERT_TRUE(sourceModel->moveRow(sourceIndex({3}), 0, sourceIndex({1, 1}), 0));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "30", "110", "111", "112", "12", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 0, 1, 1, 2, 2, 2, 2, 1, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, true, false, true, false, false, false, false, false, false, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {false, true, false, true, false, false, false, false, false, false, false}));

    // Linearization list model sends this kind of dataChanged for all possibly affected rows.
    ASSERT_EQ(dataChanges, QList<DataChange>({ //< Ranges are sorted by LinearizationListModel.
        {{LLM::LevelRole, LLM::HasChildrenRole, LLM::ExpandedRole}, "3", "7"},
        {{LLM::LevelRole, LLM::HasChildrenRole, LLM::ExpandedRole}, "10"}}));

    ASSERT_EQ(testIndex1, testModel->index(4));
    ASSERT_EQ(testIndex2, testModel->index(3));
    ASSERT_EQ(testIndex3, testModel->index(10));

    dataChanges.clear();

    const QPersistentModelIndex testIndex4(testModel->index(4)); // "30", being moved.
    const QPersistentModelIndex testIndex5(testModel->index(3));
    const QPersistentModelIndex testIndex6(testModel->index(7));

    // Move visible row within the same parent.
    ASSERT_TRUE(sourceModel->moveRow(sourceIndex({1, 1}), 0, sourceIndex({1, 1}), 4));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "110", "111", "112", "30", "12", "2", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({
        {{LLM::LevelRole, LLM::HasChildrenRole, LLM::ExpandedRole}, "3", "7"}}));
    ASSERT_EQ(testIndex4, testModel->index(7));
    ASSERT_EQ(testIndex5, testModel->index(3));
    ASSERT_EQ(testIndex6, testModel->index(6));

    dataChanges.clear();

    const QPersistentModelIndex testIndex7(testModel->index(8)); // "12", being moved.
    const QPersistentModelIndex testIndex8(testModel->index(0));
    const QPersistentModelIndex testIndex9(testModel->index(9));

    // Move visible row to invisible parent ("12" into "00") - row removal.
    ASSERT_TRUE(sourceModel->moveRow(sourceIndex({1}), 2, sourceIndex({0, 0}), 0));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "110", "111", "112", "30", "2", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({
        {{LLM::LevelRole, LLM::HasChildrenRole, LLM::ExpandedRole}, "1", "7"}}));
    ASSERT_EQ(testIndex7, QModelIndex());
    ASSERT_EQ(testIndex8, testModel->index(0));
    ASSERT_EQ(testIndex9, testModel->index(8));

    dataChanges.clear();

    // Move invisible row to visible expanded parent ("01" into "11") - row insertion.
    ASSERT_TRUE(sourceModel->moveRow(sourceIndex({0}), 1, sourceIndex({1, 1}), 0));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "01", "110", "111", "112", "30", "2", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({
        {{LLM::LevelRole, LLM::HasChildrenRole, LLM::ExpandedRole}, "0"},
        {{LLM::LevelRole, LLM::HasChildrenRole, LLM::ExpandedRole}, "3", "8"}}));

    // Collapse "1" for further testing.
    testModel->setData(testModel->index(1), false, LLM::ExpandedRole);
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "1", "2", "3"}));
    dataChanges.clear();

    // Move invisible row to invisible parent ("01" into "00") - no action.
    ASSERT_TRUE(sourceModel->moveRow(sourceIndex({1, 1}), 0, sourceIndex({0, 0}), 1));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "1", "2", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>()); //< No affected parents are visible.

    // Move invisible row to visible collapsed parent ("30" into "0") - no action.
    ASSERT_TRUE(sourceModel->moveRow(sourceIndex({1, 1}), 3, sourceIndex({0}), 1));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "1", "2", "3"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({
        {{LLM::LevelRole, LLM::HasChildrenRole, LLM::ExpandedRole}, "0"}}));

    dataChanges.clear();

    // Move visible rows to visible collapsed parent ("3" into "1") - expand the new parent.
    ASSERT_TRUE(sourceModel->moveRow({}, 3, sourceIndex({1}), 2));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "10", "11", "110", "111", "112", "3", "2"}));
    ASSERT_EQ(dataChanges, QList<DataChange>({
        {{LLM::LevelRole, LLM::HasChildrenRole, LLM::ExpandedRole}, "0", "8"}}));
}

TEST_F(LinearizationListModelTest, autoExpand)
{
    // Any role can be used to store auto-expand flag.
    // Lets use Qt::WhatsThisRole for simplicity, it already has standard name "whatsThis".

    // Mark "1:1", "1:1:0" and "2" as auto-expanded (for the future).
    sourceModel->item(1)->child(1)->setData(true, Qt::WhatsThisRole);
    sourceModel->item(1)->child(1)->child(0)->setData(true, Qt::WhatsThisRole);
    sourceModel->item(2)->setData(true, Qt::WhatsThisRole);

    // Mark "0" as auto-expanded.
    sourceModel->item(0)->setData(true, Qt::WhatsThisRole);

    // Enable auto-expand functionality by specifying its role name.
    // "0" should get expanded, "1:1" shouldn't as "1" is collapsed.
    testModel->setAutoExpandRoleName("whatsThis");

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "00", "01", "1", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0, 1, 1, 0, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, false, false, false}));

    // Add new item into invisible "1:2", no visible children inserted, no auto-expansion attempted.
    sourceModel->item(1)->child(2)->appendRow(new QStandardItem("120"));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({ "0", "00", "01", "1", "2", "3" }));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({ 0, 1, 1, 0, 0, 0 }));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        { true, false, false, true, false, true }));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        { true, false, false, false, false, false }));

    // Expand "1", "1:1" should get auto-expanded.
    testModel->setData(testModel->index(3), true, LLM::ExpandedRole);

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "10", "11", "110", "111", "112", "12", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 2, 2, 2, 1, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true, false, false, false, true, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, true, false, false, false, false, false, false}));

    // Add item to the "1:1:1" parent, no expansion should occur.
    sourceModel->item(1)->child(1)->child(1)->appendRow(new QStandardItem("1110"));
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "10", "11", "110", "111", "112", "12", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 2, 2, 2, 1, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true, false, true, false, true, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, true, false, false, false, false, false, false}));

    // Add an item to the "1:1:0" parent, it should get auto-expanded.
    sourceModel->item(1)->child(1)->child(0)->appendRow(new QStandardItem("1100"));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "10", "11", "110", "1100", "111", "112", "12", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 2, 3, 2, 2, 1, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, true, true, false, true, false, true, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, true, true, false, false, false, false, false, false}));

    // Move "3:0" into "2", it should get auto-expanded.
    sourceModel->moveRows(sourceIndex({3}), 0, 1, sourceIndex({2}), 0);

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "1", "10", "11", "110", "1100", "111", "112", "12", "2", "30", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 2, 3, 2, 2, 1, 0, 1, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>({true, false, false, true, false,
        true, true, false, true, false, true, true, false, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({true, false, false, true, false,
        true, true, false, false, false, false, true, false, false}));

    // Collapse all, expand "2", then move invisible branch "11" into expanded "2".
    // Should recursively auto-expand inserted branch, i.e. expand 11 and 110.
    testModel->collapseAll();
    testModel->setData(testModel->index(2), true, LLM::ExpandedRole);
    sourceModel->moveRows(sourceIndex({1}), 1, 1, sourceIndex({2}), 0);

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "1", "2", "11", "110", "1100", "111", "112", "30", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 0, 0, 1, 2, 3, 2, 2, 1, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, true, true, true, true, false, true, false, false, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {false, false, true, true, true, false, false, false, false, false}));
}

} // namespace test
} // namespace nx::vms::client::desktop
