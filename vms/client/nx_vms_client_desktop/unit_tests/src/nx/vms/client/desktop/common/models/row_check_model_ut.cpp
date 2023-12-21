// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QAbstractListModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtTest/QAbstractItemModelTester>

#include <nx/utils/log/format.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/desktop/common/models/row_check_model.h>

namespace nx::vms::client::desktop::test {

namespace {

// A mock source model class for RowCheckModel that implements Qt list model.
class MockSourceModel: public QAbstractListModel
{
public:
    MockSourceModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_data.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 1;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid())
            return QVariant();

        const auto row = index.row();
        if (!qBetween(0, row, rowCount()))
            return QVariant();

        switch(role)
        {
            case Qt::DisplayRole:
                return QString::number(m_data[row]);
            default:
                return QVariant();
        }
    }

    void set(int row, int value)
    {
        m_data[row] = value;
        emit dataChanged(index(row, 0), index(row, 0), {Qt::DisplayRole});
    }

    void reset(std::vector<int> elements)
    {
        beginResetModel();

        m_data = elements;

        endResetModel();
    }

    void insert(int destRow, std::vector<int> elements)
    {
        beginInsertRows({}, destRow, destRow + elements.size() - 1);

        m_data.insert(
            m_data.begin() + destRow,
            elements.begin(),
            elements.end());

        endInsertRows();
    }

    void remove(int row, int number)
    {
        beginRemoveRows({}, row, row + number - 1);

        m_data.erase(
            m_data.begin() + row,
            m_data.begin() + row + number);

        endRemoveRows();
    }

    void move(int srcRow, int number, int destRow)
    {
        beginMoveRows({}, srcRow, srcRow + number - 1, {}, destRow);

        if (destRow < srcRow)
        {
            const std::vector<int> movingRangeCopy(
                m_data.begin() + srcRow,
                m_data.begin() + srcRow + number);

            m_data.erase(
                m_data.begin() + srcRow,
                m_data.begin() + srcRow + number);

            m_data.insert(
                m_data.begin() + destRow,
                movingRangeCopy.begin(),
                movingRangeCopy.end());
        }
        else if (srcRow + number - 1 < destRow)
        {
            m_data.insert(
                m_data.begin() + destRow,
                m_data.begin() + srcRow,
                m_data.begin() + srcRow + number);

            m_data.erase(
                m_data.begin() + srcRow,
                m_data.begin() + srcRow + number);
        }

        endMoveRows();
    }

private:
    std::vector<int> m_data;
};

class RowCheckModelTest: public testing::Test
{
protected:
    void SetUp() override
    {
        sourceModel = std::make_unique<MockSourceModel>();
        model = std::make_unique<RowCheckModel>();

        m_sourceModelTester = std::make_unique<QAbstractItemModelTester>(
            sourceModel.get(),
            QAbstractItemModelTester::FailureReportingMode::Fatal);

        m_modelTester = std::make_unique<QAbstractItemModelTester>(
            model.get(),
            QAbstractItemModelTester::FailureReportingMode::Fatal);

        model->setSourceModel(sourceModel.get());
    }

    void TearDown() override
    {
        m_modelTester.reset();
        m_sourceModelTester.reset();
        model.reset();
        sourceModel.reset();
    }

    void whenSourceModelIsReset(const std::vector<int>& values)
    {
        sourceModel->reset(values);
    }

    void thenDataAreUpdated(
        const std::vector<int>& sourceValues,
        const std::vector<Qt::CheckState>& checkStateValues)
    {
        NX_ASSERT(sourceValues.size() == checkStateValues.size());
        ASSERT_EQ(sourceModel->rowCount(), sourceValues.size());
        ASSERT_EQ(model->rowCount(), sourceValues.size());

        for (int i = 0; i < sourceValues.size(); ++i)
        {
            ASSERT_EQ(
                model->index(i, 0).data(Qt::CheckStateRole).value<Qt::CheckState>(),
                checkStateValues[i]);
            ASSERT_EQ(model->index(i, 1).data().toString(), QString::number(sourceValues[i]));
        }
    };

    void thenSourceDataAreUpdated(const std::vector<int>& sourceValues)
    {
        ASSERT_EQ(sourceModel->rowCount(), sourceValues.size());
        ASSERT_EQ(model->rowCount(), sourceValues.size());

        for (int i = 0; i < sourceValues.size(); ++i)
        {
            ASSERT_EQ(model->index(i, 1).data().toString(), QString::number(sourceValues[i]));
        }
    };

    void thenCheckStatesAreUpdated(const std::vector<Qt::CheckState>& checkStateValues)
    {
        ASSERT_EQ(model->rowCount(), checkStateValues.size());

        for (int i = 0; i < checkStateValues.size(); ++i)
        {
            ASSERT_EQ(
                model->index(i, 0).data(Qt::CheckStateRole).value<Qt::CheckState>(),
                checkStateValues[i]);
        }
    };

    std::unique_ptr<MockSourceModel> sourceModel;
    std::unique_ptr<RowCheckModel> model;

private:
    std::unique_ptr<QAbstractItemModelTester> m_sourceModelTester;
    std::unique_ptr<QAbstractItemModelTester> m_modelTester;
};

} // namespace

TEST_F(RowCheckModelTest, ResetCheck)
{
    auto whenCheckStateValuesAreUpdated = [&]()
        {
            model->setData(model->index(1, 0), Qt::Checked, Qt::CheckStateRole);
        };

    // ---------------------------------------------

    whenSourceModelIsReset({1, 2, 3});
    thenDataAreUpdated({1, 2, 3}, {Qt::Unchecked, Qt::Unchecked, Qt::Unchecked});

    whenCheckStateValuesAreUpdated();
    thenDataAreUpdated({1, 2, 3}, {Qt::Unchecked, Qt::Checked, Qt::Unchecked});

    whenSourceModelIsReset({2, 3, 4, 5});
    thenDataAreUpdated(
        {2, 3, 4, 5},
        {Qt::Unchecked, Qt::Unchecked, Qt::Unchecked, Qt::Unchecked});
}

TEST_F(RowCheckModelTest, InsertRemoveMoveCheck)
{
    auto checkStateIsSet = [&]()
        {
            model->setData(model->index(1, 0), Qt::Checked, Qt::CheckStateRole);
        };

    auto whenRowIsInserted = [&]()
        {
            sourceModel->insert(0, {1});
        };

    auto whenRowIsRemoved = [&]()
        {
            sourceModel->remove(0, 1);
        };

    auto whenRowIsMovedToTheBeginning = [&]()
        {
            sourceModel->move(1, 1, 0);
        };

    auto whenRowIsMovedToTheEnd = [&]()
        {
            sourceModel->move(0, 1, 3);
        };

    // ---------------------------------------------

    whenSourceModelIsReset({1, 2, 3});
    checkStateIsSet();
    thenDataAreUpdated({1, 2, 3}, {Qt::Unchecked, Qt::Checked, Qt::Unchecked});

    whenRowIsInserted();
    thenDataAreUpdated(
        {1, 1, 2, 3},
        {Qt::Unchecked, Qt::Unchecked, Qt::Checked, Qt::Unchecked});

    whenRowIsRemoved();
    thenDataAreUpdated({1, 2, 3}, {Qt::Unchecked, Qt::Checked, Qt::Unchecked});

    whenRowIsMovedToTheBeginning();
    thenDataAreUpdated({2, 1, 3}, {Qt::Checked, Qt::Unchecked, Qt::Unchecked});

    whenRowIsMovedToTheEnd();
    thenDataAreUpdated({1, 3, 2}, {Qt::Unchecked, Qt::Unchecked, Qt::Checked});
}

TEST_F(RowCheckModelTest, GetSelectedRowsCheck)
{
    auto selectedRowsAreAbsent = [&]()
        {
            ASSERT_EQ(model->checkedRows(), QList<int>{});
        };

    auto whenRowIsSelected = [&](int rowIndex)
        {
            model->setData(model->index(rowIndex, 0), Qt::Checked, Qt::CheckStateRole);
        };

    auto selectedRowsListIsUpdated = [&](const QList<int>& selectedIndexes)
        {
            ASSERT_EQ(model->checkedRows(), selectedIndexes);
        };

    auto whenFirstRowMoved = [&]()
        {
            sourceModel->move(0, 1, 2);
        };

    auto whenAllRowsAreRemoved = [&]()
        {
            sourceModel->remove(0, 3);
        };

    // ---------------------------------------------

    whenSourceModelIsReset({1, 2, 3});
    selectedRowsAreAbsent();

    whenRowIsSelected(0);
    selectedRowsListIsUpdated({0});

    whenFirstRowMoved();
    selectedRowsListIsUpdated({1});

    whenRowIsSelected(0);
    selectedRowsListIsUpdated({0, 1});

    whenAllRowsAreRemoved();
    selectedRowsListIsUpdated({});
}

TEST_F(RowCheckModelTest, SourceDataChangedCheck)
{
    auto whenSourceRowValueIsSet = [&](int row, int value)
        {
            sourceModel->set(row, value);
        };

    // ---------------------------------------------

    whenSourceModelIsReset({1, 2, 3});
    thenSourceDataAreUpdated({1, 2, 3});

    whenSourceRowValueIsSet(0, 4);
    thenSourceDataAreUpdated({4, 2, 3});
}

TEST_F(RowCheckModelTest, DataChangedCheck)
{
    auto whenRowIsSelected = [&](int rowIndex)
        {
            model->setData(model->index(rowIndex, 0), Qt::Checked, Qt::CheckStateRole);
        };

    auto whenRowIsUnselected = [&](int rowIndex)
        {
            model->setData(model->index(rowIndex, 0), Qt::Unchecked, Qt::CheckStateRole);
        };

    // ---------------------------------------------

    whenSourceModelIsReset({1, 2, 3});
    thenCheckStatesAreUpdated({Qt::Unchecked, Qt::Unchecked, Qt::Unchecked});

    whenRowIsSelected(0);
    thenCheckStatesAreUpdated({Qt::Checked, Qt::Unchecked, Qt::Unchecked});

    whenRowIsUnselected(0);
    thenCheckStatesAreUpdated({Qt::Unchecked, Qt::Unchecked, Qt::Unchecked});
}

TEST_F(RowCheckModelTest, LayoutChangedCheck)
{
    QSortFilterProxyModel sortModel;
    sortModel.setSourceModel(sourceModel.get());
    model->setSourceModel(&sortModel);

    QPersistentModelIndex persistentIndex;

    auto whenPersistentIndexIsCreated = [&](int rowIndex)
        {
            persistentIndex = model->index(rowIndex, 1);
        };

    auto thenPersistentIndexRowIs = [&](int rowIndex, const QString& indexValue)
        {
            ASSERT_EQ(persistentIndex.row(), rowIndex);
            ASSERT_EQ(persistentIndex.data(Qt::DisplayRole).toString(), indexValue);
        };

    auto whenDataIsInserted = [&](int row, const std::vector<int>& data)
        {
            sourceModel->insert(row, data);
        };

    auto whenModelIsSorted = [&]()
        {
            sortModel.sort(0);
        };

    auto thenSourceModelDataAreUpdated = [&](const std::vector<int>& values)
        {
            ASSERT_EQ(sourceModel->rowCount(), values.size());

            for (int i = 0; i < values.size(); ++i)
            {
                ASSERT_EQ(sourceModel->index(i, 0).data().toString(), QString::number(values[i]));
            }
        };

    auto thenModelDataAreUpdated = [&](const std::vector<int>& values)
        {
            ASSERT_EQ(model->rowCount(), values.size());

            for (int i = 0; i < values.size(); ++i)
            {
                ASSERT_EQ(model->index(i, 1).data().toString(), QString::number(values[i]));
            }
        };

    // ---------------------------------------------

    whenSourceModelIsReset({1, 3, 5});
    whenPersistentIndexIsCreated(1);
    thenPersistentIndexRowIs(1, "3");

    whenDataIsInserted(0, {2, 4});
    whenModelIsSorted();
    thenSourceModelDataAreUpdated({2, 4, 1, 3, 5});
    thenModelDataAreUpdated({1, 2, 3, 4, 5});
    thenPersistentIndexRowIs(2, "3");
}

} // namespace nx::vms::client::desktop::test
