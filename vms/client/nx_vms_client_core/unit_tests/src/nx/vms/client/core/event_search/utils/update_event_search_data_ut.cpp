// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <QtTest/QAbstractItemModelTester>

#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/test_support/item_model_signal_log.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>

#include "structures.h"

namespace nx::vms::client::core::event_search {

namespace {

using namespace std::chrono;
using FetchDirection = EventSearch::FetchDirection;

using TestItems = std::vector<Item>;

class TestItemsModel: public ScopedModelOperations<QAbstractListModel>
{
public:
    virtual int rowCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : (int) items.size();
    }

    virtual QModelIndex index(
        int row, int column = 0, const QModelIndex& parent = QModelIndex()) const
    {
        return createIndex(row, column, nullptr);
    }

    // We don't need unified data access method here.
    virtual QVariant data(const QModelIndex&, int) const { return {}; }

public:
    TestItems items;
};

template<typename FetchedContainer>
auto makeBodyData(FetchedContainer&& body)
{
    const int size = (int) body.size();

    return FetchedData<std::remove_cvref_t<FetchedContainer>>{
        .data = std::forward<FetchedContainer>(body),
        .ranges = {.body = {.offset = 0, .length = size}}};
}

} // namespace

class UpdateEventSearchDataTest: public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        model.reset(new TestItemsModel());
        tester.reset(new QAbstractItemModelTester(model.get(),
            QAbstractItemModelTester::FailureReportingMode::Fatal));
        signalLog.reset(new nx::utils::test::ItemModelSignalLog(model.get()));
    }

    virtual void TearDown() override
    {
        signalLog.reset();
        tester.reset();
        model.reset();
    }

protected:
    std::unique_ptr<TestItemsModel> model;
    std::unique_ptr<QAbstractItemModelTester> tester;
    std::unique_ptr<nx::utils::test::ItemModelSignalLog> signalLog;
};

TEST_F(UpdateEventSearchDataTest, createNew)
{
    model->items = TestItems{
        Item::create("c", 3ms),
        Item::create("b", 2ms),
        Item::create("a", 1ms)};

    const TestItems newData = model->items;

    auto fetchedData = makeBodyData(newData);

    updateEventSearchData<Accessor>(model.get(),
        model->items,
        fetchedData,
        FetchDirection::newer);

    EXPECT_EQ(model->items, newData);

    EXPECT_EQ(signalLog->log(), QStringList());
}

TEST_F(UpdateEventSearchDataTest, removeAll)
{
    model->items = TestItems{
        Item::create("c", 3ms),
        Item::create("b", 2ms),
        Item::create("a", 1ms)};

    const TestItems newData;

    auto fetchedData = makeBodyData(newData);

    updateEventSearchData<Accessor>(model.get(),
        model->items,
        fetchedData,
        FetchDirection::newer);

    EXPECT_EQ(model->items, newData);

    EXPECT_EQ(signalLog->log(), QStringList({
        "rowsAboutToBeRemoved(0,2) |3",
        "rowsRemoved(0,2) |0"}));
}

TEST_F(UpdateEventSearchDataTest, nothingChanges)
{
    const TestItems newData{
        Item::create("c", 3ms),
        Item::create("b", 2ms),
        Item::create("a", 1ms)};

    auto fetchedData = makeBodyData(newData);

    updateEventSearchData<Accessor>(model.get(),
        model->items,
        fetchedData,
        FetchDirection::newer);

    EXPECT_EQ(model->items, newData);

    EXPECT_EQ(signalLog->log(), QStringList({
        "rowsAboutToBeInserted(0,2) |0",
        "rowsInserted(0,2) |3"}));
}

TEST_F(UpdateEventSearchDataTest, appendSome)
{
    model->items = TestItems{
        Item::create("c", 3ms)};

    const TestItems newData{
        Item::create("e", 5ms),
        Item::create("d", 4ms),
        Item::create("c", 3ms),
        Item::create("b", 2ms),
        Item::create("a", 1ms)};

    auto fetchedData = makeBodyData(newData);

    updateEventSearchData<Accessor>(model.get(),
        model->items,
        fetchedData,
        FetchDirection::newer);

    EXPECT_EQ(model->items, newData);

    EXPECT_EQ(signalLog->log(), QStringList({
        "rowsAboutToBeInserted(0,1) |1",
        "rowsInserted(0,1) |3",
        "rowsAboutToBeInserted(3,4) |3",
        "rowsInserted(3,4) |5"}));
}

TEST_F(UpdateEventSearchDataTest, removeSome)
{
    model->items = TestItems{
        Item::create("e", 5ms),
        Item::create("d", 4ms),
        Item::create("c", 3ms),
        Item::create("b", 2ms),
        Item::create("a", 1ms)};

    const TestItems newData{
        Item::create("c", 3ms)};

    auto fetchedData = makeBodyData(newData);

    updateEventSearchData<Accessor>(model.get(),
        model->items,
        fetchedData,
        FetchDirection::newer);

    EXPECT_EQ(model->items, newData);

    EXPECT_EQ(signalLog->log(), QStringList({
        "rowsAboutToBeRemoved(0,1) |5",
        "rowsRemoved(0,1) |3",
        "rowsAboutToBeRemoved(1,2) |3",
        "rowsRemoved(1,2) |1"}));
}

// VMS-57103 regress test.
TEST_F(UpdateEventSearchDataTest, removeInsertRemoveInsertRemove)
{
    model->items = TestItems{
        Item::create("e", 5ms),
        Item::create("c", 3ms),
        Item::create("a", 1ms)};

    const TestItems newData{
        Item::create("d", 4ms),
        Item::create("b", 2ms)};

    auto fetchedData = makeBodyData(newData);

    updateEventSearchData<Accessor>(model.get(),
        model->items,
        fetchedData,
        FetchDirection::newer);

    EXPECT_EQ(model->items, newData);

    EXPECT_EQ(signalLog->log(), QStringList({
        "rowsAboutToBeRemoved(0,0) |3",
        "rowsRemoved(0,0) |2",
        "rowsAboutToBeInserted(0,0) |2",
        "rowsInserted(0,0) |3",
        "rowsAboutToBeRemoved(1,1) |3",
        "rowsRemoved(1,1) |2",
        "rowsAboutToBeInserted(1,1) |2",
        "rowsInserted(1,1) |3",
        "rowsAboutToBeRemoved(2,2) |3",
        "rowsRemoved(2,2) |2"}));
}

TEST_F(UpdateEventSearchDataTest, updateExisting)
{
    model->items = TestItems{
        Item::create("c", 3ms),
        Item::create("b", 2ms),
        Item::create("a", 1ms)};

    const TestItems newData{
        Item::create("c", 3ms, "C"),
        Item::create("b", 2ms, "B"),
        Item::create("a", 1ms, "A")};

    auto fetchedData = makeBodyData(newData);

    updateEventSearchData<Accessor>(model.get(),
        model->items,
        fetchedData,
        FetchDirection::newer);

    EXPECT_EQ(model->items, newData);

    EXPECT_EQ(signalLog->log(), QStringList({
        "dataChanged(0,0,[])",
        "dataChanged(1,1,[])",
        "dataChanged(2,2,[])"}));
}

TEST_F(UpdateEventSearchDataTest, fixUnstableSort)
{
    model->items = TestItems{ //< Same timestamps, different IDs.
        Item::create("c", 1ms),
        Item::create("b", 1ms),
        Item::create("a", 1ms)};

    const TestItems newData{
        Item::create("a", 1ms, "A"),
        Item::create("b", 1ms),
        Item::create("c", 1ms, "C")};

    const TestItems expectedData{ //< Sorted as original data.
        Item::create("c", 1ms, "C"),
        Item::create("b", 1ms),
        Item::create("a", 1ms, "A")};

    auto fetchedData = makeBodyData(newData);

    updateEventSearchData<Accessor>(model.get(),
        model->items,
        fetchedData,
        FetchDirection::newer);

    EXPECT_EQ(model->items, expectedData);

    EXPECT_EQ(signalLog->log(), QStringList({
        "dataChanged(0,0,[])",
        "dataChanged(2,2,[])"}));
}

TEST_F(UpdateEventSearchDataTest, complexMerge)
{
    model->items = TestItems{
        Item::create("f", 9ms),
        Item::create("e", 8ms),
        Item::create("d", 5ms),
        Item::create("c", 4ms),
        Item::create("b", 3ms),
        Item::create("a", 0ms)};

    const TestItems newData{
        Item::create("z", 12ms),
        Item::create("y", 10ms),
        Item::create("x", 8ms),
        Item::create("d", 7ms),
        Item::create("c", 4ms, "Name"),
        Item::create("b", 2ms),
        Item::create("a", 0ms)};

    auto fetchedData = makeBodyData(newData);

    updateEventSearchData<Accessor>(model.get(),
        model->items,
        fetchedData,
        FetchDirection::newer);

    EXPECT_EQ(model->items, newData);

    EXPECT_EQ(signalLog->log(), QStringList({
        "rowsAboutToBeInserted(0,1) |6",    //< Insert "z" (12ms) and "y" (10ms)
        "rowsInserted(0,1) |8",
        "rowsAboutToBeRemoved(2,2) |8",     //< Remove "f" (9ms)
        "rowsRemoved(2,2) |7",
        "rowsAboutToBeRemoved(2,2) |7",     //< Remove "e" (8ms)
        "rowsRemoved(2,2) |6",
        "rowsAboutToBeInserted(2,3) |6",    //< Insert "x" (8ms) and new "d" (7ms)
        "rowsInserted(2,3) |8",
        "rowsAboutToBeRemoved(4,4) |8",     //< Remove old "d" (5ms)
        "rowsRemoved(4,4) |7",
        "dataChanged(4,4,[])",              //< Update "c" (4ms)
        "rowsAboutToBeRemoved(5,5) |7",     //< Remove old "b" (3ms)
        "rowsRemoved(5,5) |6",
        "rowsAboutToBeInserted(5,5) |6",    //< Insert new "b" (2ms)
        "rowsInserted(5,5) |7"}));
}

} // namespace nx::vms::client::core::event_search
