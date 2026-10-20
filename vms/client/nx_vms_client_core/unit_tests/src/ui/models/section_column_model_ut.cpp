// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <QtCore/QSignalBlocker>
#include <QtTest/QSignalSpy>

#include <nx/vms/client/core/common/models/test_item_model.h>
#include <ui/models/section_column_model.h>

namespace nx::vms::client::core {
namespace test {

namespace {

// List source model with "name" (display) and "section" roles.
class SourceModel: public TestItemModel
{
public:
    static constexpr int kSectionRole = Qt::UserRole + 100;

    struct Item
    {
        QString name;
        QString section;
    };

public:
    explicit SourceModel(const std::vector<Item>& items)
    {
        setItemRoleNames({{Qt::DisplayRole, "name"}, {kSectionRole, "section"}});
        appendItems(items);
    }

    static QStandardItem* createItem(const Item& item)
    {
        auto result = new QStandardItem(item.name);
        result->setData(item.section, kSectionRole);
        return result;
    }

    // Replaces all items, emitting only modelAboutToBeReset() and modelReset().
    void resetItems(const std::vector<Item>& items)
    {
        beginResetModel();
        {
            const QSignalBlocker blocker(this);
            removeRows(0, rowCount());
            appendItems(items);
        }
        endResetModel();
    }

private:
    void appendItems(const std::vector<Item>& items)
    {
        for (const auto& item: items)
            appendRow(createItem(item));
    }
};

// Represents each row of the model as "<section>: <name1>, <name2>, ...".
QStringList dump(const QAbstractItemModel& model)
{
    QStringList result;
    for (int row = 0; row < model.rowCount(); ++row)
    {
        const QModelIndex index = model.index(row, 0);

        QStringList names;
        for (const auto& item: index.data(Qt::DisplayRole).toList())
            names << item.toMap().value("name").toString();

        result << QString("%1: %2")
                      .arg(index.data(SectionColumnModel::SectionRole).toString())
                      .arg(names.join(", "));
    }
    return result;
}

} // namespace

class SectionColumnModelTest: public testing::Test
{
public:
    using Item = SourceModel::Item;

    virtual void SetUp() override;

    void givenSectionProperty(const QString& sectionProperty);
    void givenSourceModel();

    // The source model shown in sections by its "section" role.
    void givenModel();

    // Model signals emitted before this call are not taken into account.
    void givenModelSignalsRecorded();

    // The source model changes the section of the item without notifying about it.
    void givenSourceSectionSilentlyChanged(int row, const QString& section);

    void whenSourceModelSet();
    void whenSourceModelCleared();
    void whenColumnsSet(int columns);
    void whenSectionPropertySet(const QString& sectionProperty);
    void whenSourceRowInserted(int row, const Item& item);
    void whenSourceRowsRemoved(int first, int count);
    void whenSourceRowMoved(int row, int destinationRow);
    void whenSourceSorted(Qt::SortOrder order);
    void whenSourceReset(const std::vector<Item>& items);
    void whenSourceSectionChanged(int row, const QString& section);
    void whenSourceNameChanged(int row, const QString& name);

    // The source model reports a change of the row with an empty list of roles.
    void whenSourceDataChangeReported(int row);

    void whenSourceModelDestroyed();

    void thenRowsAre(const QStringList& rows);
    void thenRowCount(int count);
    void thenNoIndex(int row);
    void thenRoleNamesAre(const QHash<int, QByteArray>& roleNames);
    void thenSourceModelIs(const QAbstractItemModel* sourceModel);
    void thenColumns(int columns);
    void thenSectionProperty(const QString& sectionProperty);
    void thenSection(int row, const QString& section);
    void thenNoData(int row, int role);
    void thenNoDataForInvalidIndex();
    void thenItemCount(int row, int count);

    // Items are the roles of source model rows, keyed by role names.
    void thenItem(int row, int column, const QVariantMap& item);

    void thenModelResetCount(int count);
    void thenModelAboutToBeResetCount(int count);
    void thenSourceModelChangedCount(int count);
    void thenColumnsChangedCount(int count);
    void thenSectionPropertyChangedCount(int count);
    void thenRowsInsertedCount(int count);
    void thenRowsRemovedCount(int count);
    void thenDataChangedCount(int count);
    void thenDataChanged(int signalIndex, int firstRow, int lastRow);
    void thenDataChangedRoles(int signalIndex, const QList<int>& roles);
    void thenDataChangedOnlyForValidIndexes();

public:
    std::unique_ptr<SourceModel> source;
    SectionColumnModel model;
    int expectedRowCount = 0;

    QSignalSpy modelAboutToBeResetSpy{&model, &QAbstractItemModel::modelAboutToBeReset};
    QSignalSpy modelResetSpy{&model, &QAbstractItemModel::modelReset};
    QSignalSpy rowsInsertedSpy{&model, &QAbstractItemModel::rowsInserted};
    QSignalSpy rowsRemovedSpy{&model, &QAbstractItemModel::rowsRemoved};
    QSignalSpy dataChangedSpy{&model, &QAbstractItemModel::dataChanged};
    QSignalSpy sourceModelChangedSpy{&model, &SectionColumnModel::sourceModelChanged};
    QSignalSpy columnsChangedSpy{&model, &SectionColumnModel::columnsChanged};
    QSignalSpy sectionPropertyChangedSpy{&model, &SectionColumnModel::sectionPropertyChanged};
};

void SectionColumnModelTest::SetUp()
{
    source = std::make_unique<SourceModel>(std::vector<Item>{
        {"A", "s1"},
        {"B", "s1"},
        {"C", "s1"},
        {"D", "s2"},
        {"E", "s3"},
        {"F", "s3"},
    });

    model.setColumns(2);

    // SectionColumnModel is not fully compliant with QAbstractItemModel requirements (see its
    // description), so QAbstractItemModelTester is not used here. Instead, consistency of row
    // insertion and removal notifications with the row count is checked.

    QObject::connect(&model,
        &QAbstractItemModel::rowsAboutToBeInserted,
        [this](const QModelIndex& parent, int first, int last)
        {
            EXPECT_FALSE(parent.isValid());
            EXPECT_EQ(first, model.rowCount());
            EXPECT_LE(first, last);
            expectedRowCount = model.rowCount() + (last - first + 1);
        });

    QObject::connect(&model,
        &QAbstractItemModel::rowsInserted,
        [this]() { EXPECT_EQ(expectedRowCount, model.rowCount()); });

    QObject::connect(&model,
        &QAbstractItemModel::rowsAboutToBeRemoved,
        [this](const QModelIndex& parent, int first, int last)
        {
            EXPECT_FALSE(parent.isValid());
            EXPECT_EQ(last, model.rowCount() - 1);
            EXPECT_LE(first, last);
            expectedRowCount = model.rowCount() - (last - first + 1);
        });

    QObject::connect(&model,
        &QAbstractItemModel::rowsRemoved,
        [this]() { EXPECT_EQ(expectedRowCount, model.rowCount()); });

    givenModelSignalsRecorded();
}

void SectionColumnModelTest::givenSectionProperty(const QString& sectionProperty)
{
    model.setSectionProperty(sectionProperty);
}

void SectionColumnModelTest::givenSourceModel()
{
    model.setSourceModel(source.get());
}

void SectionColumnModelTest::givenModel()
{
    givenSectionProperty("section");
    givenSourceModel();
}

void SectionColumnModelTest::givenModelSignalsRecorded()
{
    modelAboutToBeResetSpy.clear();
    modelResetSpy.clear();
    rowsInsertedSpy.clear();
    rowsRemovedSpy.clear();
    dataChangedSpy.clear();
    sourceModelChangedSpy.clear();
    columnsChangedSpy.clear();
    sectionPropertyChangedSpy.clear();
}

void SectionColumnModelTest::givenSourceSectionSilentlyChanged(int row, const QString& section)
{
    const QSignalBlocker blocker(source.get());
    source->item(row)->setData(section, SourceModel::kSectionRole);
}

void SectionColumnModelTest::whenSourceModelSet()
{
    model.setSourceModel(source.get());
}

void SectionColumnModelTest::whenSourceModelCleared()
{
    model.setSourceModel(nullptr);
}

void SectionColumnModelTest::whenColumnsSet(int columns)
{
    model.setColumns(columns);
}

void SectionColumnModelTest::whenSectionPropertySet(const QString& sectionProperty)
{
    model.setSectionProperty(sectionProperty);
}

void SectionColumnModelTest::whenSourceRowInserted(int row, const Item& item)
{
    source->insertRow(row, SourceModel::createItem(item));
}

void SectionColumnModelTest::whenSourceRowsRemoved(int first, int count)
{
    source->removeRows(first, count);
}

void SectionColumnModelTest::whenSourceRowMoved(int row, int destinationRow)
{
    source->moveRow({}, row, {}, destinationRow);
}

void SectionColumnModelTest::whenSourceSorted(Qt::SortOrder order)
{
    source->sort(0, order);
}

void SectionColumnModelTest::whenSourceReset(const std::vector<Item>& items)
{
    source->resetItems(items);
}

void SectionColumnModelTest::whenSourceSectionChanged(int row, const QString& section)
{
    source->item(row)->setData(section, SourceModel::kSectionRole);
}

void SectionColumnModelTest::whenSourceNameChanged(int row, const QString& name)
{
    source->item(row)->setText(name);
}

void SectionColumnModelTest::whenSourceDataChangeReported(int row)
{
    emit source->dataChanged(source->index(row, 0), source->index(row, 0));
}

void SectionColumnModelTest::whenSourceModelDestroyed()
{
    source.reset();
}

void SectionColumnModelTest::thenRowsAre(const QStringList& rows)
{
    EXPECT_EQ(rows, dump(model));
}

void SectionColumnModelTest::thenRowCount(int count)
{
    EXPECT_EQ(count, model.rowCount());
}

void SectionColumnModelTest::thenNoIndex(int row)
{
    EXPECT_FALSE(model.index(row, 0).isValid());
}

void SectionColumnModelTest::thenRoleNamesAre(const QHash<int, QByteArray>& roleNames)
{
    EXPECT_EQ(roleNames, model.roleNames());
}

void SectionColumnModelTest::thenSourceModelIs(const QAbstractItemModel* sourceModel)
{
    EXPECT_EQ(sourceModel, model.sourceModel());
}

void SectionColumnModelTest::thenColumns(int columns)
{
    EXPECT_EQ(columns, model.columns());
}

void SectionColumnModelTest::thenSectionProperty(const QString& sectionProperty)
{
    EXPECT_EQ(sectionProperty, model.sectionProperty());
}

void SectionColumnModelTest::thenSection(int row, const QString& section)
{
    EXPECT_EQ(section, model.index(row, 0).data(SectionColumnModel::SectionRole).toString());
}

void SectionColumnModelTest::thenNoData(int row, int role)
{
    EXPECT_FALSE(model.index(row, 0).data(role).isValid());
}

void SectionColumnModelTest::thenNoDataForInvalidIndex()
{
    EXPECT_FALSE(model.data({}, Qt::DisplayRole).isValid());
}

void SectionColumnModelTest::thenItemCount(int row, int count)
{
    EXPECT_EQ(count, model.index(row, 0).data(Qt::DisplayRole).toList().size());
}

void SectionColumnModelTest::thenItem(int row, int column, const QVariantMap& item)
{
    const auto items = model.index(row, 0).data(Qt::DisplayRole).toList();
    ASSERT_LT(column, items.size());
    EXPECT_EQ(item, items[column].toMap());
}

void SectionColumnModelTest::thenModelResetCount(int count)
{
    EXPECT_EQ(count, modelResetSpy.count());
}

void SectionColumnModelTest::thenModelAboutToBeResetCount(int count)
{
    EXPECT_EQ(count, modelAboutToBeResetSpy.count());
}

void SectionColumnModelTest::thenSourceModelChangedCount(int count)
{
    EXPECT_EQ(count, sourceModelChangedSpy.count());
}

void SectionColumnModelTest::thenColumnsChangedCount(int count)
{
    EXPECT_EQ(count, columnsChangedSpy.count());
}

void SectionColumnModelTest::thenSectionPropertyChangedCount(int count)
{
    EXPECT_EQ(count, sectionPropertyChangedSpy.count());
}

void SectionColumnModelTest::thenRowsInsertedCount(int count)
{
    EXPECT_EQ(count, rowsInsertedSpy.count());
}

void SectionColumnModelTest::thenRowsRemovedCount(int count)
{
    EXPECT_EQ(count, rowsRemovedSpy.count());
}

void SectionColumnModelTest::thenDataChangedCount(int count)
{
    EXPECT_EQ(count, dataChangedSpy.count());
}

void SectionColumnModelTest::thenDataChanged(int signalIndex, int firstRow, int lastRow)
{
    ASSERT_LT(signalIndex, dataChangedSpy.count());
    EXPECT_EQ(firstRow, dataChangedSpy[signalIndex][0].toModelIndex().row());
    EXPECT_EQ(lastRow, dataChangedSpy[signalIndex][1].toModelIndex().row());
}

void SectionColumnModelTest::thenDataChangedRoles(int signalIndex, const QList<int>& roles)
{
    ASSERT_LT(signalIndex, dataChangedSpy.count());
    EXPECT_EQ(roles, dataChangedSpy[signalIndex][2].value<QList<int>>());
}

void SectionColumnModelTest::thenDataChangedOnlyForValidIndexes()
{
    for (const auto& arguments: dataChangedSpy)
    {
        EXPECT_TRUE(arguments[0].toModelIndex().isValid());
        EXPECT_TRUE(arguments[1].toModelIndex().isValid());
    }
}

TEST_F(SectionColumnModelTest, noSourceModel)
{
    thenRowCount(0);
    thenNoIndex(0);
}

TEST_F(SectionColumnModelTest, roleNames)
{
    thenRoleNamesAre({{Qt::DisplayRole, "data"}, {SectionColumnModel::SectionRole, "section"}});
}

TEST_F(SectionColumnModelTest, setSourceModel)
{
    givenSectionProperty("section");
    givenModelSignalsRecorded();

    whenSourceModelSet();
    thenModelResetCount(1);
    thenSourceModelChangedCount(1);
    thenSourceModelIs(source.get());
    thenRowsAre({
        "s1: A, B",
        "s1: C",
        "s2: D",
        "s3: E, F",
    });

    // Setting the same model does nothing.
    whenSourceModelSet();
    thenModelResetCount(1);
    thenSourceModelChangedCount(1);

    whenSourceModelCleared();
    thenModelResetCount(2);
    thenSourceModelChangedCount(2);
    thenRowCount(0);

    // Changes in the previous source model are not tracked anymore.
    whenSourceRowInserted(0, {"X", "s0"});
    thenRowCount(0);
}

TEST_F(SectionColumnModelTest, data)
{
    givenModel();

    thenSection(0, "s1");
    thenNoData(0, Qt::DecorationRole);
    thenItemCount(0, 2);
    thenItem(0, 1, {{"name", "B"}, {"section", "s1"}});
    thenNoDataForInvalidIndex();
}

TEST_F(SectionColumnModelTest, setColumns)
{
    givenModel();
    givenModelSignalsRecorded();

    whenColumnsSet(3);
    thenColumns(3);
    thenColumnsChangedCount(1);
    thenRowsAre({
        "s1: A, B, C",
        "s2: D",
        "s3: E, F",
    });

    whenColumnsSet(1);
    thenColumnsChangedCount(2);
    thenRowsAre({
        "s1: A",
        "s1: B",
        "s1: C",
        "s2: D",
        "s3: E",
        "s3: F",
    });

    whenColumnsSet(10);
    thenRowsAre({
        "s1: A, B, C",
        "s2: D",
        "s3: E, F",
    });

    // Setting the same value does nothing.
    whenColumnsSet(10);
    thenColumnsChangedCount(3);
}

TEST_F(SectionColumnModelTest, setSectionProperty)
{
    givenModel();
    givenModelSignalsRecorded();

    // Each item gets its own section.
    whenSectionPropertySet("name");
    thenSectionProperty("name");
    thenSectionPropertyChangedCount(1);
    thenRowsAre({
        "A: A",
        "B: B",
        "C: C",
        "D: D",
        "E: E",
        "F: F",
    });

    whenSectionPropertySet("section");
    thenSectionPropertyChangedCount(2);
    thenRowsAre({
        "s1: A, B",
        "s1: C",
        "s2: D",
        "s3: E, F",
    });

    // Setting the same value does nothing.
    whenSectionPropertySet("section");
    thenSectionPropertyChangedCount(2);
}

TEST_F(SectionColumnModelTest, sectionPropertySetAfterSourceModel)
{
    givenSourceModel();

    whenSectionPropertySet("section");

    thenRowsAre({
        "s1: A, B",
        "s1: C",
        "s2: D",
        "s3: E, F",
    });
}

TEST_F(SectionColumnModelTest, sourceRowsInserted)
{
    givenModel();
    givenModelSignalsRecorded();

    // Fits into an existing row.
    whenSourceRowInserted(4, {"G", "s2"});
    thenRowsInsertedCount(0);
    thenRowsAre({
        "s1: A, B",
        "s1: C",
        "s2: D, G",
        "s3: E, F",
    });

    // Shifts items of the section.
    whenSourceRowInserted(0, {"H", "s1"});
    thenRowsInsertedCount(0);
    thenRowsAre({
        "s1: H, A",
        "s1: B, C",
        "s2: D, G",
        "s3: E, F",
    });

    // Creates a new section in the middle.
    whenSourceRowInserted(4, {"I", "s4"});
    thenRowsInsertedCount(1);
    thenRowsAre({
        "s1: H, A",
        "s1: B, C",
        "s4: I",
        "s2: D, G",
        "s3: E, F",
    });

    // Creates a new section at the end.
    whenSourceRowInserted(source->rowCount(), {"J", "s5"});
    thenRowsInsertedCount(2);
    thenRowsAre({
        "s1: H, A",
        "s1: B, C",
        "s4: I",
        "s2: D, G",
        "s3: E, F",
        "s5: J",
    });
}

TEST_F(SectionColumnModelTest, sourceRowsRemoved)
{
    givenModel();
    givenModelSignalsRecorded();

    // Removes a row.
    whenSourceRowsRemoved(2, 1);
    thenRowsRemovedCount(1);
    thenRowsAre({
        "s1: A, B",
        "s2: D",
        "s3: E, F",
    });

    // Doesn't change row count.
    whenSourceRowsRemoved(4, 1);
    thenRowsRemovedCount(1);
    thenRowsAre({
        "s1: A, B",
        "s2: D",
        "s3: E",
    });

    // Removes a whole section in the middle.
    whenSourceRowsRemoved(2, 1);
    thenRowsRemovedCount(2);
    thenRowsAre({
        "s1: A, B",
        "s3: E",
    });
    thenModelResetCount(0);

    // Removes everything.
    whenSourceRowsRemoved(0, source->rowCount());
    thenRowsRemovedCount(3);
    thenRowCount(0);

    // A workaround for QML ListView not updating `count` property.
    thenModelResetCount(1);
}

TEST_F(SectionColumnModelTest, sourceRowsMoved)
{
    givenModel();

    // Sections with the same name which are not adjacent are not merged.
    whenSourceRowMoved(5, 0);
    thenRowsAre({
        "s3: F",
        "s1: A, B",
        "s1: C",
        "s2: D",
        "s3: E",
    });

    whenSourceRowMoved(0, 6);
    thenRowsAre({
        "s1: A, B",
        "s1: C",
        "s2: D",
        "s3: E, F",
    });
}

TEST_F(SectionColumnModelTest, sourceLayoutChanged)
{
    givenModel();
    givenModelSignalsRecorded();

    whenSourceSorted(Qt::DescendingOrder);

    thenRowsAre({
        "s3: F, E",
        "s2: D",
        "s1: C, B",
        "s1: A",
    });

    // All rows are reported as changed.
    thenDataChangedCount(1);
    thenDataChanged(0, /*firstRow*/ 0, /*lastRow*/ 3);
}

TEST_F(SectionColumnModelTest, sourceModelReset)
{
    givenModel();
    givenModelSignalsRecorded();

    whenSourceReset({
        {"X", "s1"},
        {"Y", "s2"},
        {"Z", "s2"},
    });
    thenModelAboutToBeResetCount(1);
    thenModelResetCount(1);
    thenRowsAre({
        "s1: X",
        "s2: Y, Z",
    });

    whenSourceReset({});
    thenRowCount(0);
}

TEST_F(SectionColumnModelTest, sourceSectionChanged)
{
    givenModel();

    whenSourceSectionChanged(3, "s1");
    thenRowsAre({
        "s1: A, B",
        "s1: C, D",
        "s3: E, F",
    });

    whenSourceSectionChanged(4, "s4");
    thenRowsAre({
        "s1: A, B",
        "s1: C, D",
        "s4: E",
        "s3: F",
    });
}

TEST_F(SectionColumnModelTest, sourceDataChanged)
{
    givenModel();
    givenModelSignalsRecorded();

    whenSourceNameChanged(5, "F1");
    thenDataChangedCount(1);
    thenDataChanged(0, /*firstRow*/ 3, /*lastRow*/ 3);
    thenDataChangedRoles(0, {Qt::DisplayRole});

    whenSourceNameChanged(2, "C1");
    thenDataChangedCount(2);
    thenDataChanged(1, /*firstRow*/ 1, /*lastRow*/ 1);

    thenRowsAre({
        "s1: A, B",
        "s1: C1",
        "s2: D",
        "s3: E, F1",
    });
}

TEST_F(SectionColumnModelTest, sourceDataChangedWithoutRoles)
{
    givenModel();
    givenSourceSectionSilentlyChanged(3, "s1");

    // An empty roles list means that all roles may have changed, including the section one.
    whenSourceDataChangeReported(3);

    thenRowsAre({
        "s1: A, B",
        "s1: C, D",
        "s3: E, F",
    });
}

// Nothing is shown if the section property is unknown, regardless of the order in which
// properties are set and of the kind of source model change.

TEST_F(SectionColumnModelTest, unknownSectionPropertySetAfterSourceModel)
{
    givenSourceModel();
    givenSectionProperty("unknown");
    thenRowCount(0);

    whenSourceRowInserted(0, {"X", "s0"});
    thenRowCount(0);

    whenSourceReset({
        {"X", "s0"},
        {"A", "s1"},
        {"B", "s1"},
        {"C", "s1"},
        {"D", "s2"},
        {"E", "s3"},
        {"F", "s3"},
    });
    thenRowCount(0);
}

TEST_F(SectionColumnModelTest, unknownSectionPropertySetBeforeSourceModel)
{
    givenSectionProperty("unknown");
    givenSourceModel();
    thenRowCount(0);

    whenSourceRowInserted(0, {"X", "s0"});
    thenRowCount(0);

    whenSourceReset({
        {"X", "s0"},
        {"A", "s1"},
        {"B", "s1"},
        {"C", "s1"},
        {"D", "s2"},
        {"E", "s3"},
        {"F", "s3"},
    });
    thenRowCount(0);
}

TEST_F(SectionColumnModelTest, sourceDataChangedWithUnknownSectionProperty)
{
    givenSectionProperty("unknown");
    givenSourceModel();
    givenModelSignalsRecorded();

    whenSourceNameChanged(0, "A1");

    thenDataChangedOnlyForValidIndexes();
}

TEST_F(SectionColumnModelTest, sourceRowsChangedWithUnknownSectionProperty)
{
    givenSectionProperty("unknown");
    givenSourceModel();
    givenModelSignalsRecorded();

    // Nothing is shown, so nothing changes.
    whenSourceRowInserted(0, {"X", "s0"});
    whenSourceRowsRemoved(0, 1);
    whenSourceRowMoved(0, 2);

    thenRowCount(0);
    thenModelResetCount(0);
    thenRowsInsertedCount(0);
    thenRowsRemovedCount(0);
}

TEST_F(SectionColumnModelTest, sourceModelDestroyed)
{
    givenModel();

    whenSourceModelDestroyed();

    thenSourceModelIs(nullptr);
    thenRowCount(0);
}

} // namespace test
} // namespace nx::vms::client::core
