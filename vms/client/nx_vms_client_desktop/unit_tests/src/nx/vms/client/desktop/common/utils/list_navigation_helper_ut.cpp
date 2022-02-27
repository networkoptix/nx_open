// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <QtCore/QItemSelectionModel>
#include <QtCore/QSet>
#include <QtGui/QStandardItemModel>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/common/utils/list_navigation_helper.h>

namespace nx::vms::client::desktop {
namespace test {

class ListNavigationHelperTest: public testing::Test
{
public:
    std::unique_ptr<QStandardItemModel> model;
    QItemSelectionModel selectionModel;
    ListNavigationHelper navigator;

    using Move = ListNavigationHelper::Move;
    using Selection = QSet<int>;

    static constexpr int kNoIndex = -1;

    ListNavigationHelperTest()
    {
        navigator.setSelectionModel(&selectionModel);
        navigator.setPageSize(10);
    }

    virtual void tearDown()
    {
        clearModel();
    }

    void createModel(int count)
    {
        clearModel();

        model.reset(new QStandardItemModel());
        selectionModel.setModel(model.get());

        for (int i = 0; i < count; ++i)
        {
            auto item = new QStandardItem();
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            model->appendRow(item);
        }
    }

    void clearModel()
    {
        selectionModel.setModel(nullptr);
        model.reset();
    }

    int count() const
    {
        return NX_ASSERT(model) ? model->rowCount() : 0;
    }

    void setItemFlag(int index, Qt::ItemFlag flag, bool value)
    {
        if (!NX_ASSERT(index >= 0 && index < count()))
            return;

        auto item = model->item(index);
        auto flags = item->flags();
        flags.setFlag(flag, value);
        item->setFlags(flags);
    }

    void setCurrent(int row)
    {
        selectionModel.setCurrentIndex(model->index(row, 0), QItemSelectionModel::ClearAndSelect);
    }

    int current() const
    {
        const auto currentIndex = selectionModel.currentIndex();
        return currentIndex.isValid() ? currentIndex.row() : kNoIndex;
    }

    QModelIndex index(int row) const
    {
        return NX_ASSERT(model) ? model->index(row, 0) : QModelIndex();
    }

    Selection selection() const
    {
        Selection result;
        const auto selectedIndexes = selectionModel.selectedIndexes();

        for (const auto& index: selectedIndexes)
            result.insert(index.row());

        return result;
    }

    void checkSelectedLineIs(int index)
    {
        ASSERT_EQ(current(), index);
        ASSERT_EQ(selection(), Selection({index}));
    }

    void checkNothingIsSelected()
    {
        NX_ASSERT(current() == kNoIndex);
        NX_ASSERT(selection().empty());
    }
};

TEST_F(ListNavigationHelperTest, navigateTo)
{
    // Create test model with 4 items, one is disabled, one is not selectable.
    createModel(4);
    setItemFlag(2, Qt::ItemIsEnabled, false);
    setItemFlag(3, Qt::ItemIsSelectable, false);
    checkNothingIsSelected();

    // When no index is current, navigate to specified index should work normally.
    navigator.navigateTo(index(0), Qt::NoModifier);
    checkSelectedLineIs(0);

    // Just as when some index is current.
    navigator.navigateTo(index(1), Qt::NoModifier);
    checkSelectedLineIs(1);

    // Navigation to an index that is disabled does nothing.
    navigator.navigateTo(index(2), Qt::NoModifier);
    checkSelectedLineIs(1);

    // Juas as navigation to an index that is not selectable.
    navigator.navigateTo(index(3), Qt::NoModifier);
    checkSelectedLineIs(1);
}

TEST_F(ListNavigationHelperTest, moveUp)
{
    // Create test model with 5 items, first 3 are disabled.
    createModel(5);
    setItemFlag(0, Qt::ItemIsEnabled, false);
    setItemFlag(1, Qt::ItemIsEnabled, false);
    setItemFlag(2, Qt::ItemIsEnabled, false);
    checkNothingIsSelected();

    // When no index is current, move up should do nothing.
    navigator.navigate(Move::up, Qt::NoModifier);
    checkNothingIsSelected();

    // Set the last item current.
    setCurrent(4);
    checkSelectedLineIs(4);

    // Move up to a selectable item, should succeed.
    navigator.navigate(Move::up, Qt::NoModifier);
    checkSelectedLineIs(3);

    // Move up: there's no more selectable items, should do nothing.
    navigator.navigate(Move::up, Qt::NoModifier);
    checkSelectedLineIs(3);

    // Make the first item selectable and move up: should jump to it over not selectable ones.
    setItemFlag(0, Qt::ItemIsEnabled, true);
    navigator.navigate(Move::up, Qt::NoModifier);
    checkSelectedLineIs(0);

    // Move up: there's no more items, should do nothing.
    navigator.navigate(Move::up, Qt::NoModifier);
    checkSelectedLineIs(0);
}

TEST_F(ListNavigationHelperTest, moveDown)
{
    // Create test model with 5 items, last 3 are disabled.
    createModel(5);
    setItemFlag(2, Qt::ItemIsEnabled, false);
    setItemFlag(3, Qt::ItemIsEnabled, false);
    setItemFlag(4, Qt::ItemIsEnabled, false);
    checkNothingIsSelected();

    // When no index is current, move down should do nothing.
    navigator.navigate(Move::down, Qt::NoModifier);
    checkNothingIsSelected();

    // Set the first item current.
    setCurrent(0);
    checkSelectedLineIs(0);

    // Move down to a selectable item, should succeed.
    navigator.navigate(Move::down, Qt::NoModifier);
    checkSelectedLineIs(1);

    // Move down: there's no more selectable items, should do nothing.
    navigator.navigate(Move::down, Qt::NoModifier);
    checkSelectedLineIs(1);

    // Make the last item selectable and move down: should jump to it over not selectable ones.
    setItemFlag(4, Qt::ItemIsEnabled, true);
    navigator.navigate(Move::down, Qt::NoModifier);
    checkSelectedLineIs(4);

    // Move down: there's no more items, should do nothing.
    navigator.navigate(Move::down, Qt::NoModifier);
    checkSelectedLineIs(4);
}

TEST_F(ListNavigationHelperTest, moveHome)
{
    // Create test model with 5 items, first 3 are disabled.
    createModel(5);
    setItemFlag(0, Qt::ItemIsEnabled, false);
    setItemFlag(1, Qt::ItemIsEnabled, false);
    setItemFlag(2, Qt::ItemIsEnabled, false);
    checkNothingIsSelected();

    // When no index is current, move home should do nothing.
    navigator.navigate(Move::home, Qt::NoModifier);
    checkNothingIsSelected();

    // Set the last item current.
    setCurrent(4);
    checkSelectedLineIs(4);

    // Move home should select first selectable item.
    navigator.navigate(Move::home, Qt::NoModifier);
    checkSelectedLineIs(3);

    // Make the first item selectable and move home again: should jump to it.
    setItemFlag(0, Qt::ItemIsEnabled, true);
    navigator.navigate(Move::home, Qt::NoModifier);
    checkSelectedLineIs(0);
}

TEST_F(ListNavigationHelperTest, moveToEnd)
{
    // Create test model with 5 items, last 3 are disabled.
    createModel(5);
    setItemFlag(2, Qt::ItemIsEnabled, false);
    setItemFlag(3, Qt::ItemIsEnabled, false);
    setItemFlag(4, Qt::ItemIsEnabled, false);
    checkNothingIsSelected();

    // When no index is current, move to end should do nothing.
    navigator.navigate(Move::end, Qt::NoModifier);
    checkNothingIsSelected();

    // Set the first item current.
    setCurrent(0);
    checkSelectedLineIs(0);

    // Move to end should select last selectable item.
    navigator.navigate(Move::end, Qt::NoModifier);
    checkSelectedLineIs(1);

    // Make the last item selectable and move to end again: should jump to it.
    setItemFlag(4, Qt::ItemIsEnabled, true);
    navigator.navigate(Move::end, Qt::NoModifier);
    checkSelectedLineIs(4);
}

TEST_F(ListNavigationHelperTest, pageUp)
{
    // Create test model with 31 items (navigator page size is 10), some are disabled.
    createModel(31);
    setItemFlag(0, Qt::ItemIsEnabled, false);
    setItemFlag(1, Qt::ItemIsEnabled, false);
    setItemFlag(10, Qt::ItemIsEnabled, false);
    checkNothingIsSelected();
    NX_ASSERT(navigator.pageSize() == 10);

    // When no index is current, page up should do nothing.
    navigator.navigate(Move::pageUp, Qt::NoModifier);
    checkNothingIsSelected();

    // Set the last item current.
    setCurrent(30);
    checkSelectedLineIs(30);

    // Page up to a selectable item, should succeed.
    navigator.navigate(Move::pageUp, Qt::NoModifier);
    checkSelectedLineIs(20);

    // Page up: item #10 is not selectable, should select #9 (find forward).
    navigator.navigate(Move::pageUp, Qt::NoModifier);
    checkSelectedLineIs(9);

    // Page up: items #0 and #1 are not selectable, should select #2 (find backward).
    navigator.navigate(Move::pageUp, Qt::NoModifier);
    checkSelectedLineIs(2);

    // Make the first item selectable and go page up: should select #0.
    setItemFlag(0, Qt::ItemIsEnabled, true);
    navigator.navigate(Move::pageUp, Qt::NoModifier);
    checkSelectedLineIs(0);

    // Page up: there's no more items, should do nothing.
    navigator.navigate(Move::pageUp, Qt::NoModifier);
    checkSelectedLineIs(0);
}

TEST_F(ListNavigationHelperTest, pageDown)
{
    // Create test model with 31 items (navigator page size is 10), some are disabled.
    createModel(31);
    setItemFlag(20, Qt::ItemIsEnabled, false);
    setItemFlag(29, Qt::ItemIsEnabled, false);
    setItemFlag(30, Qt::ItemIsEnabled, false);
    checkNothingIsSelected();
    NX_ASSERT(navigator.pageSize() == 10);

    // When no index is current, page down should do nothing.
    navigator.navigate(Move::pageDown, Qt::NoModifier);
    checkNothingIsSelected();

    // Set the first item current.
    setCurrent(0);
    checkSelectedLineIs(0);

    // Page down to a selectable item, should succeed.
    navigator.navigate(Move::pageDown, Qt::NoModifier);
    checkSelectedLineIs(10);

    // Page down: item #20 is not selectable, should select #21 (find forward).
    navigator.navigate(Move::pageDown, Qt::NoModifier);
    checkSelectedLineIs(21);

    // Page down: items #29 and #30 are not selectable, should select #28 (find backward).
    navigator.navigate(Move::pageDown, Qt::NoModifier);
    checkSelectedLineIs(28);

    // Make the last item selectable and go page down: should select #30.
    setItemFlag(30, Qt::ItemIsEnabled, true);
    navigator.navigate(Move::pageDown, Qt::NoModifier);
    checkSelectedLineIs(30);

    // Page down: there's no more items, should do nothing.
    navigator.navigate(Move::pageDown, Qt::NoModifier);
    checkSelectedLineIs(30);
}

TEST_F(ListNavigationHelperTest, CtrlSelection)
{
    // Create test model with 5 items, last one is disabled.
    createModel(5);
    setItemFlag(4, Qt::ItemIsEnabled, false);
    checkNothingIsSelected();

    // Toggle item #1 on.
    navigator.navigateTo(index(1), Qt::ControlModifier);
    ASSERT_EQ(current(), 1);
    ASSERT_EQ(selection(), Selection({1}));

    // Toggle item #3 on.
    navigator.navigateTo(index(3), Qt::ControlModifier);
    ASSERT_EQ(current(), 3);
    ASSERT_EQ(selection(), Selection({1, 3}));

    // Toggling an item that is disabled does nothing.
    navigator.navigateTo(index(4), Qt::ControlModifier);
    ASSERT_EQ(current(), 3);
    ASSERT_EQ(selection(), Selection({1, 3}));

    // Toggle item #1 off.
    navigator.navigateTo(index(1), Qt::ControlModifier);
    ASSERT_EQ(current(), 1);
    ASSERT_EQ(selection(), Selection({3}));
}

TEST_F(ListNavigationHelperTest, ShiftSelection)
{
    // Create test model with 10 items, item #7 is disabled.
    createModel(10);
    setItemFlag(7, Qt::ItemIsEnabled, false);
    checkNothingIsSelected();

    // When no item is current, Shift-selection works as simple navigation.
    navigator.navigateTo(index(5), Qt::ShiftModifier);
    ASSERT_EQ(current(), 5);
    ASSERT_EQ(selection(), Selection({5}));

    // Select items down to the last one, the disabled one is not selected.
    navigator.navigateTo(index(9), Qt::ShiftModifier);
    ASSERT_EQ(current(), 9);
    ASSERT_EQ(selection(), Selection({5, 6, 8, 9}));

    // Select items up to the first one, the selection is from item #5, not from #9.
    navigator.navigate(Move::home, Qt::ShiftModifier);
    ASSERT_EQ(current(), 0);
    ASSERT_EQ(selection(), Selection({0, 1, 2, 3, 4, 5}));

    // Shift has a priority over Control.
    navigator.navigateTo(index(3), Qt::ControlModifier | Qt::ShiftModifier);
    ASSERT_EQ(current(), 3);
    ASSERT_EQ(selection(), Selection({3, 4, 5}));

    // Shift-navigating to a disabled item does nothing.
    navigator.navigateTo(index(7), Qt::ShiftModifier);
    ASSERT_EQ(current(), 3);
    ASSERT_EQ(selection(), Selection({3, 4, 5}));
}

TEST_F(ListNavigationHelperTest, TwoSelectionLayers)
{
    // Shift-selection affects the top layer. Simple navigation and Ctrl-selection
    // moves (appends) the top layer to the bottom layer and then affects the bottom layer.

    createModel(10);

    navigator.navigateTo(index(1), Qt::ControlModifier); //< Bottom: {1}, Top: {}
    navigator.navigateTo(index(3), Qt::ShiftModifier); //< Bottom: {1}, Top: {1,2,3}
    ASSERT_EQ(selection(), Selection({1, 2, 3}));

    navigator.navigateTo(index(5), Qt::ControlModifier); //< Bottom: {1,2,3,5}
    ASSERT_EQ(selection(), Selection({1, 2, 3, 5}));

    navigator.navigateTo(index(0), Qt::ShiftModifier); //< Bottom: {1,2,3,5}, Top: {0,1,2,3,4,5}
    ASSERT_EQ(selection(), Selection({0, 1, 2, 3, 4, 5}));

    navigator.navigateTo(index(7), Qt::ShiftModifier); //< Bottom: {1,2,3,5}, Top: {5,6,7}
    ASSERT_EQ(selection(), Selection({1, 2, 3, 5, 6, 7}));

    navigator.navigateTo(index(3), Qt::ControlModifier); //< Bottom: {1,2,5,6,7}, Top: {}
    ASSERT_EQ(selection(), Selection({1, 2, 5, 6, 7}));
    ASSERT_EQ(current(), 3);

    // Shift-selection from an item which is current but not selected selects it again.
    navigator.navigateTo(index(0), Qt::ShiftModifier); //< Bottom: {1,2,5,6,7}, Top: {0,1,2,3}
    ASSERT_EQ(selection(), Selection({0, 1, 2, 3, 5, 6, 7}));

    navigator.navigateTo(index(4), Qt::NoModifier); //< Bottom: {4}, Top: {}
    ASSERT_EQ(selection(), Selection({4}));
}

} // namespace test
} // namespace nx::vms::client::desktop
