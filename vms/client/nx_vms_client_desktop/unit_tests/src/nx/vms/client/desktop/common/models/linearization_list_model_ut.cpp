#include <gtest/gtest.h>

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QStringList>
#include <QtGui/QStandardItemModel>

#include <nx/vms/client/desktop/common/models/linearization_list_model.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

QString toString(const QModelIndex& index)
{
    if (!index.isValid())
        return "!";

    return index.parent().isValid()
        ? QString("%1:%2").arg(toString(index.parent()), QString::number(index.row()))
        : QString::number(index.row());
}

} // namespace

class LinearizationListModelTest: public testing::Test
{
public:
    virtual void SetUp() override
    {
        sourceModel.reset(new QStandardItemModel());
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

        QObject::connect(testModel.data(), &QAbstractItemModel::dataChanged,
            [this](const QModelIndex& top, const QModelIndex& bottom, const QVector<int>& roles)
            {
                if (dataRoleFilter == -1)
                    dataChanges << DataChange({roles, toString(top), toString(bottom)});
                else if (roles.empty() || roles.contains(dataRoleFilter))
                    dataChanges << DataChange({{dataRoleFilter}, toString(top), toString(bottom)});
            });

        QObject::connect(sourceModel.data(), &QAbstractItemModel::dataChanged,
            [this](const QModelIndex& top, const QModelIndex& bottom, const QVector<int>& roles)
            {
                if (dataRoleFilter == -1)
                    sourceDataChanges << DataChange({roles, toString(top), toString(bottom)});
                else if (roles.empty() || roles.contains(dataRoleFilter))
                    sourceDataChanges << DataChange({{dataRoleFilter}, toString(top), toString(bottom)});
            });

        dataRoleFilter = -1;
        sourceDataChanges.clear();
        dataChanges.clear();
    }

    virtual void TearDown() override
    {
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
        QModelIndex current;
        for (int row: indices)
            current = sourceModel->index(row, 0, current);

        return current;
    }

    using LLM = LinearizationListModel;

    QScopedPointer<QStandardItemModel> sourceModel;
    QScopedPointer<LinearizationListModel> testModel;

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
        }

        bool operator==(const DataChange& other) const
        {
            return roles == other.roles && top == other.top && bottom == other.bottom;
        }
    };

    QList<DataChange> sourceDataChanges;
    QList<DataChange> dataChanges;
    int dataRoleFilter = -1;
};

TEST_F(LinearizationListModelTest, initialState)
{
    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "1", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0, 0, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>({true, true, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({false, false, false, false}));
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

TEST_F(LinearizationListModelTest, sourceRowsInserted)
{
    sourceModel->insertRow(1, new QStandardItem("x"));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList({"0", "x", "1", "2", "3"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>({0, 0, 0, 0, 0}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>({true, false, true, false, true}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>({false, false, false, false, false}));

    sourceModel->item(1)->insertRows(0, {new QStandardItem("x0"), new QStandardItem("x1")});
    sourceModel->item(2)->child(1)->insertRow(3, new QStandardItem("113"));

    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::HasChildrenRole}, "1"}}));

    testModel->expandAll();
    dataChanges.clear();

    sourceModel->item(2)->child(1)->insertRow(4, new QStandardItem("114"));
    sourceModel->item(3)->insertRow(0, new QStandardItem("20"));
    sourceModel->item(4)->insertRow(1, new QStandardItem("31"));

    ASSERT_EQ(itemData<QString>(Qt::DisplayRole), QStringList(
        {"0", "00", "01", "x", "x0", "x1", "1", "10", "11",
            "110", "111", "112", "113", "114", "12", "2", "3", "30", "31"}));
    ASSERT_EQ(itemData<int>(LLM::LevelRole), QList<int>(
        {0, 1, 1, 0, 1, 1, 0, 1, 1, 2, 2, 2, 2, 2, 1, 0, 0, 1, 1}));
    ASSERT_EQ(itemData<bool>(LLM::HasChildrenRole), QList<bool>(
        {true, false, false, true, false, false, true, false, true,
            false, false, false, false, false, false, true, true, false, false}));
    ASSERT_EQ(itemData<bool>(LLM::ExpandedRole), QList<bool>(
        {true, false, false, true, false, false, true, false, true,
            false, false, false, false, false, false, false, true, false, false }));

    ASSERT_EQ(dataChanges, QList<DataChange>({{{LLM::HasChildrenRole}, "15"}}));
}

TEST_F(LinearizationListModelTest, sourceLayoutChanged)
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

} // namespace test
} // namespace nx::vms::client::desktop
