// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <QtCore/QSortFilterProxyModel>
#include <QtTest/QSignalSpy>

#include <nx/vms/client/core/common/models/standard_item_model.h>
#include <nx/vms/client/core/common/utils/row_count_watcher.h>

namespace nx::vms::client::core {
namespace test {

class RowCountWatcherTest: public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        model.reset(new StandardItemModel());
        model->appendRow(new QStandardItem("0"));
        model->item(0)->appendRow(new QStandardItem("00"));
        model->item(0)->appendRow(new QStandardItem("01"));
        model->appendRow(new QStandardItem("1"));
        model->item(1)->appendRow(new QStandardItem("10"));
        model->item(1)->appendRow(new QStandardItem("11"));
        model->item(1)->appendRow(new QStandardItem("12"));
        model->appendRow(new QStandardItem("2"));
        model->item(2)->appendRow(new QStandardItem("20"));

        rowCountWatcher.reset(new RowCountWatcher());
        rowCountWatcher->setModel(model.get());

        rowCountChangedSpy.reset(
            new QSignalSpy(rowCountWatcher.get(), &RowCountWatcher::rowCountChanged));
        recursiveRowCountChangedSpy.reset(
            new QSignalSpy(rowCountWatcher.get(), &RowCountWatcher::recursiveRowCountChanged));
    }

    virtual void TearDown() override
    {
        rowCountWatcher.reset();
        model.reset();
    }

public:
    std::unique_ptr<QStandardItemModel> model;
    std::unique_ptr<RowCountWatcher> rowCountWatcher;
    std::unique_ptr<QSignalSpy> rowCountChangedSpy;
    std::unique_ptr<QSignalSpy> recursiveRowCountChangedSpy;
};

class TestFilterModel: public QSortFilterProxyModel
{
public:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
    {
        if (!filteringEnabled)
            return true;

        // Filter out rows that start with "1".
        const auto sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        return !sourceIndex.data(Qt::DisplayRole).toString().startsWith("1");
    }

public:
    bool filteringEnabled = false;
};

#define ASSERT_ROW_COUNT_CHANGED() \
    ASSERT_EQ(rowCountChangedSpy->size(), 1); \
    rowCountChangedSpy->clear()

#define ASSERT_RECURSIVE_ROW_COUNT_CHANGED() \
    ASSERT_EQ(recursiveRowCountChangedSpy->size(), 1); \
    recursiveRowCountChangedSpy->clear()

#define ASSERT_ROW_COUNT_NOT_CHANGED() \
    ASSERT_TRUE(rowCountChangedSpy->empty());

#define ASSERT_RECURSIVE_ROW_COUNT_NOT_CHANGED() \
    ASSERT_TRUE(recursiveRowCountChangedSpy->empty());

TEST_F(RowCountWatcherTest, noModel)
{
    RowCountWatcher modellessWatcher;
    EXPECT_EQ(modellessWatcher.rowCount(), 0);
}

TEST_F(RowCountWatcherTest, topLevel)
{
    EXPECT_EQ(rowCountWatcher->rowCount(), 3);
}

TEST_F(RowCountWatcherTest, notTopLevel)
{
    rowCountWatcher->setParentIndex(model->index(0, 0));
    EXPECT_EQ(rowCountWatcher->rowCount(), 2);
    ASSERT_ROW_COUNT_CHANGED();

    model->removeRows(/*row*/ 0, /*count*/ 1, /*parent*/ {}); //< Remove parent row.
    EXPECT_EQ(rowCountWatcher->rowCount(), 0);
    ASSERT_ROW_COUNT_CHANGED();
}

TEST_F(RowCountWatcherTest, modelChanged)
{
    QSignalSpy modelChangedSpy(rowCountWatcher.get(), &RowCountWatcher::modelChanged);

    QStandardItemModel otherModel;
    otherModel.appendRow(new QStandardItem());

    rowCountWatcher->setModel(&otherModel);
    EXPECT_EQ(rowCountWatcher->rowCount(), 1);
    ASSERT_ROW_COUNT_CHANGED();
    EXPECT_EQ(modelChangedSpy.size(), 1);
}

TEST_F(RowCountWatcherTest, parentIndexChanged)
{
    QSignalSpy parentIndexChangedSpy(rowCountWatcher.get(), &RowCountWatcher::parentIndexChanged);

    rowCountWatcher->setParentIndex(model->index(1, 0));
    EXPECT_EQ(rowCountWatcher->rowCount(), 3);
    ASSERT_ROW_COUNT_NOT_CHANGED();
    EXPECT_EQ(parentIndexChangedSpy.size(), 1);
    parentIndexChangedSpy.clear();

    rowCountWatcher->setParentIndex(model->index(0, 0));
    EXPECT_EQ(rowCountWatcher->rowCount(), 2);
    ASSERT_ROW_COUNT_CHANGED();
    EXPECT_EQ(parentIndexChangedSpy.size(), 1);
}

TEST_F(RowCountWatcherTest, independentModelAndParentIndex)
{
    // `model` and `parentIndex` can be set in any order.
    // `parentIndex.model()` != `model` is a valid case, albeit temporary.

    QSignalSpy parentIndexChangedSpy(rowCountWatcher.get(), &RowCountWatcher::parentIndexChanged);
    QSignalSpy modelChangedSpy(rowCountWatcher.get(), &RowCountWatcher::modelChanged);

    QStandardItemModel otherModel;
    otherModel.appendRow(new QStandardItem());
    otherModel.item(0)->appendRow(new QStandardItem());

    rowCountWatcher->setParentIndex(model->index(0, 0));
    ASSERT_ROW_COUNT_CHANGED();
    EXPECT_TRUE(modelChangedSpy.empty());
    EXPECT_EQ(parentIndexChangedSpy.size(), 1);
    parentIndexChangedSpy.clear();
    modelChangedSpy.clear();

    // Change `model` to `otherModel`; `parentIndex` stays of the old model.
    rowCountWatcher->setModel(&otherModel);
    EXPECT_EQ(rowCountWatcher->rowCount(), 0);
    ASSERT_ROW_COUNT_CHANGED();
    EXPECT_EQ(modelChangedSpy.size(), 1);
    EXPECT_TRUE(parentIndexChangedSpy.empty());
    parentIndexChangedSpy.clear();
    modelChangedSpy.clear();

    // Now change `parentIndex` properly.
    rowCountWatcher->setParentIndex(otherModel.index(0, 0));
    EXPECT_EQ(rowCountWatcher->rowCount(), 1);
    ASSERT_ROW_COUNT_CHANGED();
    EXPECT_TRUE(modelChangedSpy.empty());
    EXPECT_EQ(parentIndexChangedSpy.size(), 1);
    parentIndexChangedSpy.clear();
    modelChangedSpy.clear();

    // Change `parentIndex` to the first model; `model` stays `otherModel`.
    rowCountWatcher->setParentIndex(model->index(1, 0));
    EXPECT_EQ(rowCountWatcher->rowCount(), 0);
    ASSERT_ROW_COUNT_CHANGED();
    EXPECT_TRUE(modelChangedSpy.empty());
    EXPECT_EQ(parentIndexChangedSpy.size(), 1);
    parentIndexChangedSpy.clear();
    modelChangedSpy.clear();

    // Change `model` to the first model; now `parentIndex` matches it.
    rowCountWatcher->setModel(model.get());
    EXPECT_EQ(rowCountWatcher->rowCount(), 3);
    ASSERT_ROW_COUNT_CHANGED();
    EXPECT_EQ(modelChangedSpy.size(), 1);
    EXPECT_TRUE(parentIndexChangedSpy.empty());
    parentIndexChangedSpy.clear();
    modelChangedSpy.clear();
}

TEST_F(RowCountWatcherTest, modelReset)
{
    model->clear();
    EXPECT_EQ(rowCountWatcher->rowCount(), 0);
    ASSERT_ROW_COUNT_CHANGED();
}

TEST_F(RowCountWatcherTest, rowsInserted)
{
    model->appendRow(new QStandardItem("3"));
    EXPECT_EQ(rowCountWatcher->rowCount(), 4);
    ASSERT_ROW_COUNT_CHANGED();
}

TEST_F(RowCountWatcherTest, rowsRemoved)
{
    model->removeRow(0);
    EXPECT_EQ(rowCountWatcher->rowCount(), 2);
    ASSERT_ROW_COUNT_CHANGED();
}

TEST_F(RowCountWatcherTest, rowsMoved)
{
    model->moveRow(/*sourceParent*/{}, 2, /*destinationParent*/ model->index(0, 0), 0);
    EXPECT_EQ(rowCountWatcher->rowCount(), 2);
    ASSERT_ROW_COUNT_CHANGED();

    model->moveRow(/*sourceParent*/ model->index(0, 0), 0, /*destinationParent*/ {}, 0);
    EXPECT_EQ(rowCountWatcher->rowCount(), 3);
    ASSERT_ROW_COUNT_CHANGED();
}

TEST_F(RowCountWatcherTest, layoutChanged)
{
    TestFilterModel filterModel;
    filterModel.setSourceModel(model.get());

    rowCountWatcher->setModel(&filterModel);
    ASSERT_ROW_COUNT_NOT_CHANGED(); //< No filtering yet.

    QSignalSpy layoutChangeChecker(&filterModel, &QAbstractItemModel::layoutChanged);
    filterModel.filteringEnabled = true;
    filterModel.invalidate(); //< Does full layout change.

    // Ensure that the current implementation of QSortFilterProxyModel indeed does layout change,
    // so this test remains valid.
    ASSERT_EQ(layoutChangeChecker.size(), 1);

    EXPECT_EQ(rowCountWatcher->rowCount(), 2);
    ASSERT_ROW_COUNT_CHANGED();
}

TEST_F(RowCountWatcherTest, recursiveTracking)
{
    EXPECT_EQ(rowCountWatcher->recursiveRowCount(), 0);
    EXPECT_EQ(rowCountWatcher->rowCount(), 3);

    rowCountWatcher->setRecursiveTracking(true);
    ASSERT_ROW_COUNT_NOT_CHANGED();
    ASSERT_RECURSIVE_ROW_COUNT_CHANGED();
    EXPECT_EQ(rowCountWatcher->recursiveRowCount(), 9);

    model->moveRow(model->index(0, 0), 0, model->index(1, 0), 0);
    ASSERT_RECURSIVE_ROW_COUNT_NOT_CHANGED();
    EXPECT_EQ(rowCountWatcher->recursiveRowCount(), 9);

    model->moveRow(model->index(1, 0), 0, QModelIndex{}, 3);
    ASSERT_RECURSIVE_ROW_COUNT_NOT_CHANGED();
    EXPECT_EQ(rowCountWatcher->recursiveRowCount(), 9);
    ASSERT_ROW_COUNT_CHANGED();
    EXPECT_EQ(rowCountWatcher->rowCount(), 4);

    model->removeRow(0, model->index(1, 0));
    ASSERT_RECURSIVE_ROW_COUNT_CHANGED();
    EXPECT_EQ(rowCountWatcher->recursiveRowCount(), 8);
    ASSERT_ROW_COUNT_NOT_CHANGED();

    model->removeRow(3);
    ASSERT_RECURSIVE_ROW_COUNT_CHANGED();
    EXPECT_EQ(rowCountWatcher->recursiveRowCount(), 7);
    ASSERT_ROW_COUNT_CHANGED();
    EXPECT_EQ(rowCountWatcher->rowCount(), 3);

    model->item(2)->child(0)->appendRow(new QStandardItem());
    ASSERT_RECURSIVE_ROW_COUNT_CHANGED();
    EXPECT_EQ(rowCountWatcher->recursiveRowCount(), 8);
    ASSERT_ROW_COUNT_NOT_CHANGED();

    rowCountWatcher->setParentIndex(model->index(2, 0));
    ASSERT_RECURSIVE_ROW_COUNT_CHANGED();
    ASSERT_ROW_COUNT_CHANGED();
    EXPECT_EQ(rowCountWatcher->recursiveRowCount(), 2);
    EXPECT_EQ(rowCountWatcher->rowCount(), 1);

    rowCountWatcher->setRecursiveTracking(false);
    ASSERT_RECURSIVE_ROW_COUNT_CHANGED();
    ASSERT_ROW_COUNT_NOT_CHANGED();
    EXPECT_EQ(rowCountWatcher->recursiveRowCount(), 0);
}

} // namespace test
} // namespace nx::vms::client::core
