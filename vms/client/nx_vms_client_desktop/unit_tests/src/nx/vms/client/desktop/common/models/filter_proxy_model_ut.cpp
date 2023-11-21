// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include <QtCore/QPointer>
#include <QtCore/QSortFilterProxyModel> //< For layout change testing.
#include <QtTest/QAbstractItemModelTester>

#include <nx/utils/log/format.h>
#include <nx/utils/debug_helpers/model_transaction_checker.h>
#include <nx/vms/client/desktop/common/models/filter_proxy_model.h>

#include "test_item_model.h"

namespace nx::vms::client::desktop {
namespace test {

class FilterProxyModelTest: public testing::Test
{
public:
    virtual void SetUp() override
    {
        sourceModel.reset(new TestItemModel());

        sourceModel->appendRow(new QStandardItem("Orthoceratoidea"));
        sourceModel->item(0)->appendRow(new QStandardItem("Orthocerida"));
        sourceModel->item(0)->appendRow(new QStandardItem("Dissidocerida"));
        sourceModel->item(0)->appendRow(new QStandardItem("Pseudorthocerida"));
        sourceModel->item(0)->appendRow(new QStandardItem("Ascocerida"));
        sourceModel->item(0)->appendRow(new QStandardItem("Lituitida"));

        sourceModel->appendRow(new QStandardItem("Nautiloidea"));
        sourceModel->item(1)->appendRow(new QStandardItem("Yanhecerida"));
        sourceModel->item(1)->appendRow(new QStandardItem("Endocerida"));
        sourceModel->item(1)->appendRow(new QStandardItem("Tarphycerida"));
        sourceModel->item(1)->appendRow(new QStandardItem("Oncocerida"));
        sourceModel->item(1)->appendRow(new QStandardItem("Discosorida"));
        sourceModel->item(1)->appendRow(new QStandardItem("Nautilida"));

        sourceModel->appendRow(new QStandardItem("Ammonoidea"));
        sourceModel->item(2)->appendRow(new QStandardItem("Agoniatitida"));
        sourceModel->item(2)->appendRow(new QStandardItem("Ammonitida"));
        sourceModel->item(2)->appendRow(new QStandardItem("Ceratitida"));
        sourceModel->item(2)->appendRow(new QStandardItem("Clymeniida"));
        sourceModel->item(2)->appendRow(new QStandardItem("Goniatitida"));

        sourceModel->appendRow(new QStandardItem("Coleoidea"));
        sourceModel->item(3)->appendRow(new QStandardItem("Belemnoidea"));
        sourceModel->item(3)->child(0)->appendRow(new QStandardItem("Aulacocerida"));
        sourceModel->item(3)->child(0)->appendRow(new QStandardItem("Phragmoteuthida"));
        sourceModel->item(3)->child(0)->appendRow(new QStandardItem("Hematitida"));
        sourceModel->item(3)->child(0)->appendRow(new QStandardItem("Belemnitida"));
        sourceModel->item(3)->appendRow(new QStandardItem("Neocoleoidea"));

        testModel.reset(new FilterProxyModel());
        testModel->setDebugChecksEnabled(true);
        testModel->setSourceModel(sourceModel.get());

        modelTester.reset(new QAbstractItemModelTester(testModel.get(),
            QAbstractItemModelTester::FailureReportingMode::Fatal));

        transactionChecker = nx::utils::ModelTransactionChecker::install(testModel.get());
    }

    virtual void TearDown() override
    {
        modelTester.reset();
        testModel.reset();
        sourceModel.reset();
    }

    QString modelToString() const
    {
        return recursiveIndexToString({});
    }

    QList<QPersistentModelIndex> getAllIndexes() const
    {
        QList<QPersistentModelIndex> result;
        getAllIndexesRecursively(result, {});
        return result;
    }

    QString getText(const QList<QPersistentModelIndex>& indexes) const
    {
        QStringList result;
        std::transform(indexes.cbegin(), indexes.cend(), std::back_inserter(result),
            [](const QModelIndex& index) { return index.data(Qt::DisplayRole).toString(); });
        return result.join(", ");
    }

    void setPassFilter(const QVector<char>& charsToStartWith, QAbstractItemModel* model = nullptr)
    {
        if (!model)
            model = sourceModel.get();

        testModel->setFilter(
            [model, charsToStartWith]
                (int sourceRow, const QModelIndex& sourceParent) -> bool
            {
                const auto sourceIndex = model->index(sourceRow, 0, sourceParent);
                const auto text = sourceIndex.data(Qt::DisplayRole).toString();
                return std::any_of(charsToStartWith.cbegin(), charsToStartWith.cend(),
                    [text](char c) { return text.startsWith(c); });
            });
    }

    void setPassFilterWithException(const QVector<char>& charsToStartWith,
        const QString& parentWithNotFilteredChildren,
        QAbstractItemModel* model = nullptr)
    {
        if (!model)
            model = sourceModel.get();

        testModel->setFilter(
            [charsToStartWith, parentWithNotFilteredChildren, model](
                int sourceRow, const QModelIndex& sourceParent) -> bool
            {
                if (sourceParent.data(Qt::DisplayRole).toString() == parentWithNotFilteredChildren)
                    return true;

                const auto sourceIndex = model->index(sourceRow, 0, sourceParent);
                const auto text = sourceIndex.data(Qt::DisplayRole).toString();
                return std::any_of(charsToStartWith.cbegin(), charsToStartWith.cend(),
                    [text](char c) { return text.startsWith(c); });
            });
    }

private:
    QString recursiveIndexToString(const QModelIndex& index) const
    {
        QString result;
        if (index.isValid())
            result = index.data(Qt::DisplayRole).toString();

        const auto children = recursiveChildrenToString(index);
        if (children.isEmpty())
            return result;

        return result.isEmpty()
            ? children
            : nx::format("%1 {%2}", result, children).toQString();
    }

    QString recursiveChildrenToString(const QModelIndex& index) const
    {
        const int count = testModel->rowCount(index);
        if (count == 0)
            return {};

        QStringList children;
        children.reserve(count);

        for (int i = 0; i < count; ++i)
            children.push_back(recursiveIndexToString(testModel->index(i, 0, index)));

        return children.join(", ");
    }

    void getAllIndexesRecursively(
        QList<QPersistentModelIndex>& result, const QModelIndex& parent) const
    {
        for (int row = 0, n = testModel->rowCount(parent); row < n; ++row)
        {
            const auto index = testModel->index(row, 0, parent);
            result.push_back(index);
            getAllIndexesRecursively(result, index);
        }
    }

public:
    std::unique_ptr<TestItemModel> sourceModel;
    std::unique_ptr<FilterProxyModel> testModel;
    std::unique_ptr<QAbstractItemModelTester> modelTester;
    QPointer<nx::utils::ModelTransactionChecker> transactionChecker;
};

// -----------------------------------------------------------------------------------------------
//

TEST_F(FilterProxyModelTest, initialState)
{
    const auto defaultContent = QString(
        "Orthoceratoidea {Orthocerida, Dissidocerida, Pseudorthocerida, Ascocerida, Lituitida}, "
        "Nautiloidea {Yanhecerida, Endocerida, Tarphycerida, Oncocerida, Discosorida, Nautilida}, "
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida, Goniatitida}, "
        "Coleoidea {"
            "Belemnoidea {Aulacocerida, Phragmoteuthida, Hematitida, Belemnitida}, "
            "Neocoleoidea}");

    ASSERT_EQ(modelToString(), defaultContent);
}

TEST_F(FilterProxyModelTest, simpleFilter)
{
    setPassFilter({'A', 'B', 'C'});

    const auto filteredContent = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_EQ(modelToString(), filteredContent);
}

TEST_F(FilterProxyModelTest, insertAll)
{
    sourceModel->item(1)->insertRows(1, QList<QStandardItem*>({
        new QStandardItem("Plectronocerida"),
        new QStandardItem("Ellesmerocerida")}));

    const auto newContent = QString(
        "Orthoceratoidea {Orthocerida, Dissidocerida, Pseudorthocerida, Ascocerida, Lituitida}, "
        "Nautiloidea {Yanhecerida, Plectronocerida, Ellesmerocerida, Endocerida, Tarphycerida, "
            "Oncocerida, Discosorida, Nautilida}, "
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida, Goniatitida}, "
        "Coleoidea {"
            "Belemnoidea {Aulacocerida, Phragmoteuthida, Hematitida, Belemnitida}, "
            "Neocoleoidea}");

    ASSERT_EQ(modelToString(), newContent);
}

TEST_F(FilterProxyModelTest, insertNone)
{
    setPassFilter({'A', 'B', 'C'});

    const auto filteredContent = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    // Insert rows into a filtered out parent.
    sourceModel->item(1)->insertRows(1, {
        new QStandardItem("Plectronocerida"),
        new QStandardItem("Ellesmerocerida") });

    ASSERT_EQ(modelToString(), filteredContent);

    // Insert into a remaining parent a row that will be filtered out.
    sourceModel->item(2)->insertRow(4, new QStandardItem("Prolecanitida"));

    ASSERT_EQ(modelToString(), filteredContent);

    // Insert into a remaining parent some rows that will be completely filtered out.
    sourceModel->item(2)->child(1)->insertRows(0, {
        new QStandardItem("Lytoceratina"),
        new QStandardItem("Phylloceratina")});

    ASSERT_EQ(modelToString(), filteredContent);
}

TEST_F(FilterProxyModelTest, insertSome)
{
    setPassFilter({'A', 'B', 'C', 'E', 'N', 'Y'});

    sourceModel->item(1)->insertRows(1, {
        new QStandardItem("Plectronocerida"),
        new QStandardItem("Ellesmerocerida") });

    const auto newContent1 = QString(
        "Nautiloidea {Yanhecerida, Ellesmerocerida, Endocerida, Nautilida}, "
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}, Neocoleoidea}");

    ASSERT_EQ(modelToString(), newContent1);

    sourceModel->item(2)->child(1)->insertRows(0, {
        new QStandardItem("Lytoceratina"),
        new QStandardItem("Ammonitina"),
        new QStandardItem("Ancyloceratina"),
        new QStandardItem("Phylloceratina")});

    const auto newContent2 = QString(
        "Nautiloidea {Yanhecerida, Ellesmerocerida, Endocerida, Nautilida}, "
        "Ammonoidea {Agoniatitida, Ammonitida {Ammonitina, Ancyloceratina}, "
        "Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}, Neocoleoidea}");

    ASSERT_EQ(modelToString(), newContent2);
}

TEST_F(FilterProxyModelTest, removeAll)
{
    sourceModel->item(1)->removeRows(1, 4);

    const auto newContent1 = QString(
        "Orthoceratoidea {Orthocerida, Dissidocerida, Pseudorthocerida, Ascocerida, Lituitida}, "
        "Nautiloidea {Yanhecerida, Nautilida}, "
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida, Goniatitida}, "
        "Coleoidea {"
            "Belemnoidea {Aulacocerida, Phragmoteuthida, Hematitida, Belemnitida}, "
            "Neocoleoidea}");

    ASSERT_EQ(modelToString(), newContent1);

    sourceModel->removeRows(0, 3);

    const auto newContent2 = QString(
        "Coleoidea {"
            "Belemnoidea {Aulacocerida, Phragmoteuthida, Hematitida, Belemnitida}, "
            "Neocoleoidea}");

    ASSERT_EQ(modelToString(), newContent2);
}

TEST_F(FilterProxyModelTest, removeNone)
{
    setPassFilter({'A', 'B', 'C'});

    const auto filteredContent = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    // Remove rows from a filtered out parent.
    sourceModel->item(1)->removeRows(0, 5);

    ASSERT_EQ(modelToString(), filteredContent);

    // Remove filtered out rows from a remaining parent.
    sourceModel->item(3)->child(0)->removeRows(1, 2);

    ASSERT_EQ(modelToString(), filteredContent);
}

TEST_F(FilterProxyModelTest, removeSome)
{
    setPassFilter({'A', 'B', 'C', 'E', 'N', 'Y'});

    const auto newContent1 = QString(
        "Nautiloidea {Yanhecerida, Endocerida, Nautilida}, "
        "Ammonoidea {Agoniatitida, Ammonitida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}, Neocoleoidea}");

    sourceModel->item(2)->removeRows(2, 3);

    ASSERT_EQ(modelToString(), newContent1);

    sourceModel->removeRows(0, 2);

    const auto newContent2 = QString(
        "Ammonoidea {Agoniatitida, Ammonitida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}, Neocoleoidea}");

    ASSERT_EQ(modelToString(), newContent2);
}

// Move from a filtered out to a filtered out parent.
TEST_F(FilterProxyModelTest, moveAsNothing)
{
    setPassFilter({'A', 'B', 'C'});

    const auto filteredContent = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({0}), 0, 5,
        sourceModel->buildIndex({1}), 0));

    ASSERT_EQ(modelToString(), filteredContent);
}

// Move from a filtered out to a remaining parent, all moved rows are filtered out.
TEST_F(FilterProxyModelTest, moveAsInsertionOfNone)
{
    setPassFilter({'A', 'B', 'C'});

    const auto filteredContent = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({0}), 0, 3,
        sourceModel->buildIndex({2}), 0));

    ASSERT_EQ(modelToString(), filteredContent);
}

// Move from a filtered out to a remaining parent, no moved rows are filtered out.
TEST_F(FilterProxyModelTest, moveAsInsertionOfAll)
{
    setPassFilter({'A', 'B', 'C', 'Y'});

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({0}), 3, 1,
        sourceModel->buildIndex({2}), 0));

    const auto newContent1 = QString(
        "Ammonoidea {Ascocerida, Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_EQ(modelToString(), newContent1);

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({1}), 0, 1,
        sourceModel->buildIndex({2}), 6));

    const auto newContent2 = QString(
        "Ammonoidea {Ascocerida, Agoniatitida, Ammonitida, Ceratitida, Clymeniida, Yanhecerida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_EQ(modelToString(), newContent2);
}

// Move from a filtered out to a remaining parent, some of moved rows are filtered out.
TEST_F(FilterProxyModelTest, moveAsInsertionOfSome)
{
    setPassFilter({'A', 'B', 'C', 'Y'});

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({0}), 3, 2,
        sourceModel->buildIndex({2}), 0));

    const auto newContent1 = QString(
        "Ammonoidea {Ascocerida, Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_EQ(modelToString(), newContent1);

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({1}), 0, 6,
        sourceModel->buildIndex({2}), 6));

    const auto newContent2 = QString(
        "Ammonoidea {Ascocerida, Agoniatitida, Ammonitida, Ceratitida, Clymeniida, Yanhecerida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_EQ(modelToString(), newContent2);
}

// Move rows that were filtered out from a remaining parent to a filtered out parent.
TEST_F(FilterProxyModelTest, moveAsRemovalOfNone)
{
    setPassFilter({'A', 'B', 'C'});

    const auto filteredContent = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_TRUE(sourceModel->moveRows({}, 0, 2,
        sourceModel->buildIndex({3, 1}), 0));

    ASSERT_EQ(modelToString(), filteredContent);
}

// Move rows that weren't filtered out from a remaining parent to a filtered out parent.
TEST_F(FilterProxyModelTest, moveAsRemovalOfAll)
{
    setPassFilter({'A', 'B', 'C'});

    const auto newContent = QString(
        "Ammonoidea, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({2}), 0, 4,
        sourceModel->buildIndex({3, 1}), 0));

    ASSERT_EQ(modelToString(), newContent);

    ASSERT_TRUE(sourceModel->moveRows({}, 2, 2, sourceModel->buildIndex({0}), 3));

    ASSERT_EQ(modelToString(), QString());
}

// Move rows that were partially filtered out from a remaining parent to a filtered out parent.
TEST_F(FilterProxyModelTest, moveAsRemovalOfSome)
{
    setPassFilter({'A', 'B', 'C'});

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({2}), 2, 3,
        sourceModel->buildIndex({3, 1}), 0));

    const auto newContent1 = QString(
        "Ammonoidea {Agoniatitida, Ammonitida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_EQ(modelToString(), newContent1);

    ASSERT_TRUE(sourceModel->moveRows({}, 1, 2, sourceModel->buildIndex({0}), 3));

    const auto newContent2 = QString(
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_EQ(modelToString(), newContent2);
}

// Move within the same remaining parent, all moved rows are filtered out.
TEST_F(FilterProxyModelTest, shortMoveOfNone)
{
    setPassFilter({'A', 'B', 'C'});

    const auto filteredContent = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    // Forward.
    ASSERT_TRUE(sourceModel->moveRows({}, 0, 2, {}, 3));

    ASSERT_EQ(modelToString(), filteredContent);

    // Backward.
    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({2}), 4, 1,
        sourceModel->buildIndex({2}), 1));

    ASSERT_EQ(modelToString(), filteredContent);
}

// Move within the same remaining parent, none of moved rows are filtered out.
TEST_F(FilterProxyModelTest, shortMoveOfAll)
{
    setPassFilter({'A', 'B', 'C'});

    // Forward.
    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({2}), 1, 2,
        sourceModel->buildIndex({2}), 4));

    const auto newContent1 = QString(
        "Ammonoidea {Agoniatitida, Clymeniida, Ammonitida, Ceratitida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_EQ(modelToString(), newContent1);

    // Backward.
    ASSERT_TRUE(sourceModel->moveRows({}, 3, 1, {}, 2));

    const auto newContent2 = QString(
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}, "
        "Ammonoidea {Agoniatitida, Clymeniida, Ammonitida, Ceratitida}");

    ASSERT_EQ(modelToString(), newContent2);
}

// Move within the same remaining parent, some of moved rows are filtered out.
TEST_F(FilterProxyModelTest, shortMoveOfSome)
{
    setPassFilter({'A', 'B', 'C'});

    // Forward.
    ASSERT_TRUE(sourceModel->moveRows({}, 0, 3, {}, 4));

    const auto newContent1 = QString(
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}, "
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}");

    ASSERT_EQ(modelToString(), newContent1);

    // Backward.
    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({3}), 2, 3,
        sourceModel->buildIndex({3}), 1));

    const auto newContent2 = QString(
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}, "
        "Ammonoidea {Agoniatitida, Ceratitida, Clymeniida, Ammonitida}");

    ASSERT_EQ(modelToString(), newContent2);
}

// Move within the same remaining parent, rows are moved around filtered out ones, no move occurs.
TEST_F(FilterProxyModelTest, shortMoveAroundFilteredOutRows)
{
    setPassFilter({'A', 'G'});
    const auto filteredContent = QString("Ammonoidea {Agoniatitida, Ammonitida, Goniatitida}");

    // Forward.
    const auto sourceParent = sourceModel->buildIndex({2});
    ASSERT_TRUE(sourceModel->moveRows(sourceParent, 0, 2, sourceParent, 4));

    ASSERT_EQ(modelToString(), filteredContent);

    // Backward.
    ASSERT_TRUE(sourceModel->moveRows(sourceParent, 2, 2, sourceParent, 0));
    ASSERT_EQ(modelToString(), filteredContent);
}

// Move between different remaining parents, all moved rows were and will be filtered out.
TEST_F(FilterProxyModelTest, longMoveOfNone)
{
    setPassFilter({'A', 'B', 'C'});

    const auto filteredContent = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    ASSERT_TRUE(sourceModel->moveRows({}, 0, 2, sourceModel->buildIndex({2}), 0));

    ASSERT_EQ(modelToString(), filteredContent);

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({1}), 1, 1, {}, 0));

    ASSERT_EQ(modelToString(), filteredContent);
}

// Move between different remaining parents, no moved rows were and will be filtered out.
TEST_F(FilterProxyModelTest, longMoveOfAll)
{
    setPassFilter({'A', 'B', 'C'});

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({3, 0}), 0, 1,
        sourceModel->buildIndex({2}), 2));

    const auto newContent1 = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Aulacocerida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Belemnitida}}");

    ASSERT_EQ(modelToString(), newContent1);

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({2}), 0, 5,
        sourceModel->buildIndex({3}), 1));

    const auto newContent2 = QString(
        "Ammonoidea, Coleoidea {Belemnoidea {Belemnitida}, Agoniatitida, Ammonitida, "
            "Aulacocerida, Ceratitida, Clymeniida}");

    ASSERT_EQ(modelToString(), newContent2);
}

// Move between different remaining parents, some of moved rows were and will be filtered out.
// No filter change: inserted and removed counts are the same.
TEST_F(FilterProxyModelTest, longMoveOfSome)
{
    setPassFilter({'A', 'B', 'C'});

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({3, 0}), 0, 4,
        sourceModel->buildIndex({2}), 2));

    const auto newContent1 = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Aulacocerida, Belemnitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea}");

    ASSERT_EQ(modelToString(), newContent1);

    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({2}), 0, 9,
        sourceModel->buildIndex({3}), 1));

    const auto newContent2 = QString(
        "Ammonoidea, Coleoidea {Belemnoidea, Agoniatitida, Ammonitida, Aulacocerida, Belemnitida, "
            "Ceratitida, Clymeniida}");

    ASSERT_EQ(modelToString(), newContent2);
}

// Move between different remaining parents, due to the filter change inserted and removed counts
// are different.
TEST_F(FilterProxyModelTest, longMoveFilterChanged)
{
    setPassFilterWithException({'A', 'B', 'C', 'N', 'Y'}, "Ammonoidea");

    const auto initialFilteredContent = QString(
        "Nautiloidea {Yanhecerida, Nautilida}, "
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida, Goniatitida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}, Neocoleoidea}");

    ASSERT_EQ(modelToString(), initialFilteredContent);

    // No removal, only insertion.
    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({1}), 1, 2,
        sourceModel->buildIndex({2}), 5));

    const auto newContent1 = QString(
        "Nautiloidea {Yanhecerida, Nautilida}, "
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida, Goniatitida, "
            "Endocerida, Tarphycerida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}, Neocoleoidea}");

    ASSERT_EQ(modelToString(), newContent1);

    // Both removal and insertion.
    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({2}), 1, 4,
        sourceModel->buildIndex({1}), 0));

    const auto newContent2 = QString(
        "Nautiloidea {Ammonitida, Ceratitida, Clymeniida, Yanhecerida, Nautilida}, "
        "Ammonoidea {Agoniatitida, Endocerida, Tarphycerida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}, Neocoleoidea}");

    ASSERT_EQ(modelToString(), newContent2);

    // Only removal, no insertion.
    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({2}), 1, 2,
        sourceModel->buildIndex({1}), 3));

    const auto newContent3 = QString(
        "Nautiloidea {Ammonitida, Ceratitida, Clymeniida, Yanhecerida, Nautilida}, "
        "Ammonoidea {Agoniatitida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}, Neocoleoidea}");

    ASSERT_EQ(modelToString(), newContent3);
}

TEST_F(FilterProxyModelTest, layoutChanges)
{
    QSortFilterProxyModel layoutChanger;
    layoutChanger.setSourceModel(sourceModel.get());
    testModel->setSourceModel(&layoutChanger);

    setPassFilter({'A', 'B', 'C'}, &layoutChanger);

    const auto filteredContent = QString(
        "Ammonoidea {Agoniatitida, Ammonitida, Ceratitida, Clymeniida}, "
        "Coleoidea {Belemnoidea {Aulacocerida, Belemnitida}}");

    const auto persistentIndexList = getAllIndexes();
    const QString textFromIndexes = "Ammonoidea, Agoniatitida, Ammonitida, Ceratitida, "
        "Clymeniida, Coleoidea, Belemnoidea, Aulacocerida, Belemnitida";

    // Check initial state.
    ASSERT_EQ(modelToString(), filteredContent);
    ASSERT_EQ(getText(persistentIndexList), textFromIndexes);

    // Vertical sort layout change.
    layoutChanger.sort(0, Qt::DescendingOrder);
    layoutChanger.setDynamicSortFilter(true);

    const auto newContent1 = QString(
        "Coleoidea {Belemnoidea {Belemnitida, Aulacocerida}}, "
        "Ammonoidea {Clymeniida, Ceratitida, Ammonitida, Agoniatitida}");

    const QString newTextFromIndexes1 = "Coleoidea, Belemnoidea, Belemnitida, Aulacocerida, "
        "Ammonoidea, Clymeniida, Ceratitida, Ammonitida, Agoniatitida";

    ASSERT_EQ(modelToString(), newContent1);
    ASSERT_EQ(getText(persistentIndexList), textFromIndexes); //< Must persist.
    ASSERT_EQ(getText(getAllIndexes()), newTextFromIndexes1);

    // Rows moved layout change.
    ASSERT_TRUE(sourceModel->moveRows(sourceModel->buildIndex({2}), 2, 3,
        sourceModel->buildIndex({3, 0}), 2));

    const auto newContent2 = QString(
        "Coleoidea {Belemnoidea {Clymeniida, Ceratitida, Belemnitida, Aulacocerida}}, "
        "Ammonoidea {Ammonitida, Agoniatitida}");

    const QString newTextFromIndexes2 = "Coleoidea, Belemnoidea, Clymeniida, Ceratitida, "
        "Belemnitida, Aulacocerida, Ammonoidea, Ammonitida, Agoniatitida";

    ASSERT_EQ(modelToString(), newContent2);
    ASSERT_EQ(getText(persistentIndexList), textFromIndexes); //< Must persist.
    ASSERT_EQ(getText(getAllIndexes()), newTextFromIndexes2);
}

TEST_F(FilterProxyModelTest, invalidIndex)
{
    auto index = testModel->index(sourceModel->rowCount(), 0);
    ASSERT_EQ(index, QModelIndex());

    index = testModel->index(0, sourceModel->columnCount());
    ASSERT_EQ(index, QModelIndex());
}

} // namespace test
} // namespace nx::vms::client::desktop
