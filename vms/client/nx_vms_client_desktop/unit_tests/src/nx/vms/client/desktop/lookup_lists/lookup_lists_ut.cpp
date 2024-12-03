// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QFileInfo>
#include <QString>

#include <gtest/gtest.h>

#include <nx/utils/test_support/test_options.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <nx/vms/client/core/analytics/taxonomy/state_view.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_entries_sort_filter_proxy_model.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_export_processor.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_model.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_preview_processor.h>

#include "taxonomy_based_ut.h"

using namespace nx::vms::client::core::analytics::taxonomy;
using namespace nx::vms::api;
namespace nx::vms::client::desktop {
class LookupListTests: public TaxonomyBasedTest
{
public:
    void checkValidationOfEmptyRowWithOneIncorrect(
        const QString& attributeName, const QString& incorrect, const QString& correct,
        const std::optional<LookupListData>& testData = {})
    {
        const int initialRowCount =
            givenImportModel(testData.value_or(bigColumnNumberExampleData()));

        // Create invalid entry for Generic List and attempt to add it to the model.
        whenAddEntriesByImportModel({{{attributeName, incorrect}}});

        // Check that number of rows didn't change
        thenImportModelRowCountIs(initialRowCount);

        // Check fixup is required.
        thenFixUpIsRequired();

        // Apply fix for incorrect attribute.
        whenApplyFixUp(attributeName, {{incorrect, correct}});

        // Check that number of rows increased to one
        thenImportModelRowCountIs(initialRowCount + 1);
    }

    void checkValidationOfRowsWithCorrectAndIncorrectValues(
        const QString& attributeNameOfIncorrectVal,
        const std::optional<LookupListData>& testData = {})
    {
        std::vector<QVariantMap> entriesToAdd;
        std::map<QString, QString> incorrectToCorrect;
        givenRowsWithAllCorrectValuesExceptOne(
            entriesToAdd, incorrectToCorrect, attributeNameOfIncorrectVal, 5);

        const int initialRowCount =
            givenImportModel(testData.value_or(bigColumnNumberExampleData()));

        // Attempt to add number of entries to the model
        whenAddEntriesByImportModel(entriesToAdd);

        // Check that number of rows increased, since there are valid values in row
        const int newNumberOfRows = initialRowCount + entriesToAdd.size();

        thenImportModelRowCountIs(newNumberOfRows);
        thenFixUpIsRequired();

        // Apply fixes for incorrect values
        whenApplyFixUp(attributeNameOfIncorrectVal, incorrectToCorrect);

        // Check that number of rows didn't change
        thenImportModelRowCountIs(newNumberOfRows);
    }

    void checkRemovingOfAttribute(const QString& columnToDelete)
    {
        givenLookupListEntriesModel(lookupListWithAttributesAndAdditionalRows(columnToDelete));

        const auto initalRowNumbers = m_entriesModel->rowCount();
        ASSERT_GT(initalRowNumbers, 0);

        // Removing one column header(attribute) from LookupListModel.
        whenRemoveColumn(columnToDelete);

        // Check that previously added entry with one column is deleted,
        // since this row is now comletely empty.
        const auto newRowCountNumber = initalRowNumbers - 1;
        thenRowCountIs(newRowCountNumber, m_entriesModel.get());

        // Check that entries of LookupLists doesn't contain deleted column.
        thenModelHasNoColumn(columnToDelete);

        // Return column back again.
        whenAddColumn(columnToDelete);

        // Check that number of rows didn't change.
        thenRowCountIs(newRowCountNumber, m_entriesModel.get());

        // Check that all entries has empty value of column, that was previously deleted.
        thenColumnValuesAreEmpty(columnToDelete);
    }

    void checkBuildingLookupListPreviewModel(
        const LookupListData& exportedData, const LookupListData& modelImportTo)
    {
        givenLookupListEntriesModel(exportedData);
        givenImportModel(modelImportTo);

        whenBuildTablePreview();
        thenImportedValuesAreValid(exportedData, modelImportTo);
    }

protected:
    int givenImportModel(const LookupListData& data, StateView* taxonomy = nullptr)
    {
        m_importModel = std::make_shared<LookupListImportEntriesModel>();
        auto lookupList = new LookupListModel(data, m_importModel.get());
        auto entriesModel = new LookupListEntriesModel(m_importModel.get());
        entriesModel->setListModel(lookupList);

        entriesModel->setTaxonomy(taxonomy ? taxonomy : stateView());
        m_importModel->setLookupListEntriesModel(entriesModel);

        return m_importModel->lookupListEntriesModel()->rowCount();
    }

    void givenRowsWithAllCorrectValuesExceptOne(std::vector<QVariantMap>& resultRows,
        std::map<QString, QString>& resultFixes,
        const QString& attributeName,
        int numberOfRows)
    {
        // Prepare rows with all correct values, except one
        for (int i = 0; i < numberOfRows; i++)
        {
            QVariantMap entry = correctRow();
            const QString incorrectVal = QString::number(i) + "_incorrect";
            entry[attributeName] = incorrectVal;
            // Init fix for incorrect val.
            resultFixes[incorrectVal] = correctRowToStdMap()[attributeName];
            resultRows.push_back(entry);
        }
    }


    void givenLookupListEntriesProxyModel(const LookupListData& data)
    {
        givenLookupListEntriesModel(data);
        m_entriesProxyModel = std::make_shared<LookupListEntriesSortFilterProxyModel>();
        m_entriesProxyModel->setSourceModel(m_entriesModel.get());
    }

    void givenLookupListEntriesModel(LookupListModel* lookupList)
    {
        m_entriesModel = std::make_shared<LookupListEntriesModel>();
        m_entriesModel->setListModel(lookupList);
        m_entriesModel->setTaxonomy(stateView());
    }

    void givenLookupListEntriesModel(const LookupListData& data, bool allowEmpty = false)
    {
        m_entriesModel = std::make_shared<LookupListEntriesModel>();
        auto lookupList = new LookupListModel(data, m_entriesModel.get());
        m_entriesModel->setListModel(lookupList);
        m_entriesModel->setTaxonomy(stateView());
        if (!allowEmpty)
            ASSERT_GT(m_entriesModel->rowCount(), 0);
    }

    void whenAddEntries(const std::vector<QVariantMap>& entries)
    {
        for (auto& entry: entries)
            m_entriesModel->addEntry(entry);
    }

    void whenAddEntriesByImportModel(std::vector<QVariantMap> entries)
    {
        for (auto& row: entries)
            m_importModel->addLookupListEntry(row);
    }

    void whenApplyFixUp(
        const QString& attributeName, const std::map<QString, QString>& incorrectToFix)
    {
        for (auto& [incorrect, correct]: incorrectToFix)
            m_importModel->addFixForWord(attributeName, incorrect, correct);
        m_importModel->applyFix();
    }

    void whenRemoveColumn(const std::optional<QString>& columnToDelete = {})
    {
        auto columnHeaders = m_entriesModel->listModel()->attributeNames();
        ASSERT_FALSE(columnHeaders.empty());
        if (columnToDelete)
            columnHeaders.removeAll(columnToDelete);
        else
            columnHeaders.removeFirst();
        const auto columnCountBefore = m_entriesModel->columnCount();
        m_entriesModel->listModel()->setAttributeNames(columnHeaders);
        ASSERT_EQ(m_entriesModel->columnCount(), columnCountBefore - 1);
    }

    void whenAddColumn(const QString& column)
    {
        auto columnHeaders = m_entriesModel->listModel()->attributeNames();
        ASSERT_FALSE(columnHeaders.empty());
        columnHeaders.push_back(column);
        const auto columnCountBefore = m_entriesModel->columnCount();
        m_entriesModel->listModel()->setAttributeNames(columnHeaders);

        ASSERT_EQ(m_entriesModel->columnCount(), columnCountBefore + 1);
    }

    void whenSetNewLookupListModel(const LookupListData& data)
    {
        auto newLookupListModel = new LookupListModel(data, m_entriesModel.get());
        m_entriesModel->setListModel(newLookupListModel);
    }

    void whenExportModelToFile()
    {
        m_exportTestFile.setFileName(TestOptions::temporaryDirectoryPath(true) + "/"
            + m_entriesModel->listModel()->rawData().name + ".csv");

        ASSERT_TRUE(m_exportTestFile.open(QFile::WriteOnly));

        // Export LookupListEntriesModel.
        QTextStream exportStream(&m_exportTestFile);
        m_entriesModel->exportEntries({}, exportStream);
        exportStream.flush();

        ASSERT_GT(m_exportTestFile.size(), 0);

        m_exportTestFile.close();
    }

    void whenBuildTablePreview(LookupListPreviewProcessor::PreviewBuildResult code =
        LookupListPreviewProcessor::PreviewBuildResult::Success, bool resetHeader = true)
    {
        whenExportModelToFile();

        LookupListPreviewProcessor previewProcessor;
        const auto buildTablePreviewResult = previewProcessor.buildTablePreview(
            m_importModel.get(), m_exportTestFile.fileName(), ",", true, resetHeader);
        ASSERT_EQ(buildTablePreviewResult, code);
        m_exportTestFile.remove();
    }

    void whenRevertImport() { m_importModel->revertImport(); }

    void whenSetFilter(const QString& filter)
    {
        m_entriesProxyModel->setFilterRegularExpression(filter);
    }

    void whenSetAttributesNames(const QList<QString>& attributesNames)
    {
        m_entriesModel->listModel()->setAttributeNames(attributesNames);
    }

    void whenSortingByColumn(int column, Qt::SortOrder order)
    {
        m_entriesProxyModel->sort(column, order);
    }

    void whenSetAttributesPosition(const std::vector<QString>& selectedChoice)
    {
        for (int i = 0; i < selectedChoice.size(); ++i)
            m_importModel->headerIndexChanged(i, selectedChoice[i]);
    }

    void thenBuildPreviewHeaderEqualTo(const std::vector<QString>& selectedNames)
    {
        const auto attributeMapping = m_importModel->columnIndexToAttribute();

        ASSERT_EQ(m_entriesModel->columnCount(), selectedNames.size());
        for (int i = 0; i < selectedNames.size(); ++i)
        {
            EXPECT_EQ(selectedNames[i], attributeMapping[i])
                << "Expect to have: " << selectedNames[i].toStdString() << " on position: " << i;
            auto headerData = m_importModel->headerData(i, Qt::Horizontal).value<QList<QString>>();
            EXPECT_FALSE(headerData.empty());
            EXPECT_EQ(selectedNames[i], headerData.front());
        }
    }

    void thenListEntriesHasAttributes(const QList<QString>& attributesNames)
    {
        ASSERT_EQ(m_entriesModel->listModel()->attributeNames(), attributesNames);

        thenColumnCountIs(attributesNames.size(), m_entriesModel.get());
        thenHeaderDataEqualTo(m_entriesModel.get(), attributesNames);

        for (const auto& entry: m_entriesModel->listModel()->rawData().entries)
        {
            for (const auto& [attribute, _]: entry)
                ASSERT_TRUE(attributesNames.contains(attribute));
        }
    }

    void thenHeaderDataEqualTo(QAbstractItemModel* model, const QList<QString>& data)
    {
        int valueToCheckLeft = data.size();
        for (int column = 0; column < m_entriesModel->columnCount(); ++column)
        {
            if (data.contains(m_entriesModel->headerData(column).toString()))
                valueToCheckLeft -= 1;
        }
        ASSERT_EQ(valueToCheckLeft, 0);
    }

    void thenFixUpIsRequired() { ASSERT_TRUE(m_importModel->fixupRequired()); }
    void thenFixUpIsNotRequired() { ASSERT_FALSE(m_importModel->fixupRequired()); }

    void thenImportModelRowCountIs(int rowCount)
    {
        thenRowCountIs(rowCount, m_importModel->lookupListEntriesModel());
    }

    void thenRowCountIs(int rowCount, QAbstractItemModel* model)
    {
        ASSERT_EQ(rowCount, model->rowCount());
    }

    void thenColumnCountIs(int columnCount, QAbstractTableModel* model)
    {
        ASSERT_EQ(columnCount, model->columnCount());
    }

    void thenModelHasNoColumn(const QString& columnName)
    {
        for (auto& entry: m_entriesModel->listModel()->rawData().entries)
            ASSERT_FALSE(entry.contains(columnName));
    }

    void thenColumnValuesAreEmpty(const QString& columnName)
    {
        for (auto& entry: m_entriesModel->listModel()->rawData().entries)
            ASSERT_TRUE(entry[columnName].isEmpty());
    }

    void thenImportedValuesAreValid(
        const LookupListData& exportedData, const LookupListData& modelImportTo)
    {
        // Check that preview model contains the same amount of rows as in exported model.
        thenRowCountIs(exportedData.entries.size(), m_importModel.get());

        // Check that preview model contains the same amount of columns as exported model.
        thenColumnCountIs(exportedData.attributeNames.size(), m_importModel.get());

        for (int c = 0; c < m_importModel->columnCount(); ++c)
        {
            auto curHeader = m_importModel->headerData(c, Qt::Horizontal);

            // Check that attribute name, by position i in LookupListModel
            // is on first position of i column
            // OR "Do not import" text on additional columns.
            ASSERT_TRUE(curHeader.isValid());
            ASSERT_FALSE(curHeader.toList().empty());
            ASSERT_EQ(curHeader.toList().front().toString(),
                c < modelImportTo.attributeNames.size() ? modelImportTo.attributeNames[c]
                                                        : m_importModel->doNotImportText());

            for (int r = 0; r < exportedData.entries.size(); ++r)
            {
                // Check that data in importModel is the same as in imported file.
                ASSERT_EQ(m_importModel->index(r, c).data(),
                    m_entriesModel->index(r, c).data(
                        LookupListEntriesModel::DataRole::RawValueRole));
            }
        }
    }

    QVariant whenRequestingColorHex(int row, int column)
    {
        return m_entriesModel->data(m_entriesModel->index(row, column),
            LookupListEntriesModel::DataRole::ColorRGBHexValueRole);
    }

    QVariant whenRequestingColorDisplayValue(int row, int column)
    {
        return m_entriesModel->data(m_entriesModel->index(row, column));
    }

    void thenColorHexIsValidOrEmpty(int row, int column, QVariant value)
    {
        const auto attributeName = m_entriesModel->data(m_entriesModel->index(row, column),
            LookupListEntriesModel::DataRole::AttributeNameRole);
        const auto rawValue = m_entriesModel->data(m_entriesModel->index(row, column),
             LookupListEntriesModel::DataRole::RawValueRole).toString();
        if (attributeName == "Color" && !rawValue.isEmpty())
        {
            ASSERT_TRUE(value.toString().startsWith("#"))
                << "Expected the value to start with '#', but received:"
                << value.toString().toStdString();
        }
        else
        {
            ASSERT_TRUE(value.isNull());
        }
    }

    void thenColorHexIsEmpty(QVariant value)
    {
        ASSERT_TRUE(value.isNull())
            << "Expected empty value, but received:" << value.toString().toStdString();
    }

    void thenColorNameIsValid(int row, int column, QVariant value)
    {
        const auto attributeName = m_entriesModel->data(m_entriesModel->index(row, column),
            LookupListEntriesModel::DataRole::AttributeNameRole);
        const auto rawValue = m_entriesModel->data(m_entriesModel->index(row, column),
             LookupListEntriesModel::DataRole::RawValueRole).toString();
        const auto displayedValue = value.toString();
        if (attributeName == "Color" && !rawValue.isEmpty())
            ASSERT_FALSE(displayedValue.startsWith("#"));
        else
            ASSERT_FALSE(displayedValue.isEmpty());
    }

    void thenExportedDataIsTheSameAsInModel()
    {
        ASSERT_TRUE(m_exportTestFile.open(QIODevice::ReadOnly));

        int row = -1;
        while (!m_exportTestFile.atEnd())
        {
            const auto lineData = m_exportTestFile.readLine().split(',');

            ASSERT_EQ(lineData.size(), m_entriesModel->columnCount());

            int column = 0;
            for (QString value: lineData)
            {
                if (row == -1) //< The header of file is equal to headerData
                {
                    ASSERT_EQ(value.trimmed(), m_entriesModel->headerData(column).toString());
                }
                else //< The row data is the same as in model.
                {
                    const auto modelIndex = m_entriesModel->index(row, column);
                    ASSERT_TRUE(modelIndex.isValid());
                    ASSERT_EQ(value.trimmed(),
                        modelIndex.data(LookupListEntriesModel::DataRole::RawValueRole)
                            .toString());
                }
                column += 1;
            }
            row += 1;
        }

        thenRowCountIs(row, m_entriesModel.get());
    }

    template<class T>
    void thenValuesAreOrdered(Qt::SortOrder order, int column)
    {
        QVariant previousValue;

        for (int row = 0; row < m_entriesProxyModel->rowCount(); ++row)
        {
            const auto index = m_entriesProxyModel->index(row, column);
            ASSERT_TRUE(index.isValid());
            const auto currentValue = index.data(LookupListEntriesModel::DataRole::SortRole);
            if (currentValue.isNull())
                continue;

            ASSERT_TRUE(currentValue.canConvert<T>());
            if (previousValue.isNull())
            {
                previousValue = currentValue;
                continue;
            }

            if (order == Qt::AscendingOrder)
                ASSERT_LE(previousValue.value<T>(), currentValue.value<T>());
            else
                ASSERT_GE(previousValue.value<T>(), currentValue.value<T>());
        }
    }

    void checkAnyValuesAreGrouped(QVariant& previousValue, int row, int column)
    {
        const auto index = m_entriesProxyModel->index(row, column);
        ASSERT_TRUE(index.isValid());
        bool endOfAnyValueGroup = false; //< After sorting empty values are sorted to one group.
        const auto currentValue = index.data(LookupListEntriesModel::DataRole::SortRole);
        if (currentValue.isNull())
        {
            ASSERT_FALSE(endOfAnyValueGroup);
            ASSERT_TRUE(previousValue.isNull());
        }
        else
        {
            endOfAnyValueGroup = true;
        }
        previousValue = currentValue;
    }
    void thenEmptyValuesAreAtTheBegining(int column)
    {
        QVariant previousValue =
            m_entriesProxyModel->index(0, column).data(LookupListEntriesModel::DataRole::SortRole);

        for (int row = 1; row < m_entriesProxyModel->rowCount(); ++row)
            checkAnyValuesAreGrouped(previousValue, row, column);
    }

    void thenEmptyValuesAreAtTheEnd(int column)
    {
        int rowCount = m_entriesProxyModel->rowCount();
        QVariant previousValue = m_entriesProxyModel->index(rowCount, column)
                                     .data(LookupListEntriesModel::DataRole::SortRole);

        for (int row = rowCount - 1; row >= 0; --row)
            checkAnyValuesAreGrouped(previousValue, row, column);
    }

    std::map<QString, QString> correctRowToStdMap()
    {
        std::map<QString, QString> result;
        for (auto [key, value]: correctRow().asKeyValueRange())
            result[key] = value.toString();
        return result;
    }

    QVariantMap correctRow()
    {
        QVariantMap entry;
        entry["Approved"] = "true";
        entry["IntNumberWithoutLimit"] = "1";
        entry["Color"] = "red";
        entry["Enum"] = "base enum item 1.1";
        entry["FloatNumberWithLimit"] = "3.5";
        entry["FloatNumberWithoutLimit"] = "-9.67";
        entry["FloatNumberWithMinLimit"] = "34.67";
        entry["FloatNumberWithMaxLimit"] = "0.01";
        entry["IntNumberWithLimit"] = "5";
        entry["IntNumberWithLimitBellowZero"] = "-11";
        entry["IntNumberWithMinLimit"] = "5";
        entry["IntNumberWithMaxLimit"] = "78";

        return entry;
    }

    LookupListData lookupListWithAttributesAndAdditionalRows(const QString& column)
    {
        auto data = bigColumnNumberExampleData();
        data.entries.push_back(correctRowToStdMap());
        // Adding entry with only one column specified.
        const auto correctValueOfAttribute = correctRowToStdMap()[column];
        data.entries.push_back({{column, correctRowToStdMap()[correctValueOfAttribute]}});
        return data;
    }

    LookupListData smallColumnNumberGenericExampleData()
    {
        LookupListData e2;
        e2.id = nx::Uuid::createUuid();
        e2.name = "Values";
        e2.attributeNames.push_back("Value");
        e2.entries = {{{"Value", "0"}},
            {{"Value", "1"}},
            {{"Value", "2"}},
            {{"Value", "3"}},
            {{"Value", "4"}},
            {{"Value", "5"}},
            {{"Value", "6"}}};
        return e2;
    }

    LookupListData numericColumnExampleData()
    {
        LookupListData e1;
        e1.objectTypeId = "base.object.type.1";
        e1.id = nx::Uuid::createUuid();
        e1.name = "Numbers";
        e1.attributeNames.push_back("IntNumberWithoutLimit");
        e1.attributeNames.push_back("FloatNumberWithoutLimit");
        e1.entries = {
            {{"IntNumberWithoutLimit", "-10"}},
            {{"IntNumberWithoutLimit", "100"}},
            {{"IntNumberWithoutLimit", "1"}},
            {{"IntNumberWithoutLimit", "5"}},
            {{"IntNumberWithoutLimit", "90"}},
            {{"IntNumberWithoutLimit", "90"}},
            {{"IntNumberWithoutLimit", "-1"}},
            {{"IntNumberWithoutLimit", "10000"}},
            {{"IntNumberWithoutLimit", "1001"}},
            {{"FloatNumberWithoutLimit", "100.28"}},
            {{"IntNumberWithoutLimit", ""}, {"FloatNumberWithoutLimit", "1.5"}},
        };
        return e1;
    }

    LookupListData stringColumnExampleData()
    {
        LookupListData e1;
        e1.objectTypeId = "base.object.type.1";
        e1.id = nx::Uuid::createUuid();
        e1.name = "Enums";
        e1.attributeNames.push_back("StringType");
        e1.attributeNames.push_back("FloatNumberWithoutLimit");
        e1.entries = {
            {{"StringType", "10000"}},
            {{"StringType", "1001"}},
            {{"StringType", "aaaa"}},
            {{"StringType", "bbbb"}},
            {{"StringType", "c"}},
            {{"FloatNumberWithoutLimit", "100.28"}},
            {{"StringType", ""}, {"FloatNumberWithoutLimit", "1.5"}},
        };
        return e1;
    }

    LookupListData bigColumnNumberExampleData()
    {
        LookupListData e1;
        e1.objectTypeId = "base.object.type.1";
        e1.id = nx::Uuid::createUuid();
        e1.name = "Numbers";
        e1.attributeNames.push_back("IntNumberWithoutLimit");
        e1.attributeNames.push_back("FloatNumberWithLimit");
        e1.attributeNames.push_back("FloatNumberWithoutLimit");
        e1.attributeNames.push_back("FloatNumberWithMinLimit");
        e1.attributeNames.push_back("FloatNumberWithMaxLimit");
        e1.attributeNames.push_back("Color");
        e1.attributeNames.push_back("Enum");
        e1.attributeNames.push_back("Approved");
        e1.attributeNames.push_back("IntNumberWithLimit");
        e1.attributeNames.push_back("IntNumberWithLimitBellowZero");
        e1.attributeNames.push_back("IntNumberWithMinLimit");
        e1.attributeNames.push_back("IntNumberWithMaxLimit");
        e1.entries = {
            {{"IntNumberWithoutLimit", "12345"},
                {"Color", "red"},
                {"Enum", "base enum item 1.1"},
                {"Approved", "true"},
                {"IntNumberWithLimit", "19"},
                {"FloatNumberWithLimit", "5.0"},
                {"FloatNumberWithoutLimit", "9.876"},
                {"FloatNumberWithMinLimit", "1.6"},
                {"FloatNumberWithMaxLimit", "10.7"},
                {"IntNumberWithLimitBellowZero", "-13"},
                {"IntNumberWithMinLimit", "100"},
                {"IntNumberWithMaxLimit", "99"},
            },
            {{"IntNumberWithoutLimit", "67890"}, {"Approved", "false"}},
            {{"Color", "blue"}, {"IntNumberWithoutLimit", "9876"}},
            {{"IntNumberWithLimit", "4"}, {"FloatNumberWithLimit", "2"}},
            {{"Enum", "base enum item 1.2"}, {"FloatNumberWithLimit", "7"}, {"Color", "green"}},
        };
        return e1;
    }

    LookupListData derivedObjectExampleData()
    {
        LookupListData e1;
        e1.id = nx::Uuid::createUuid();
        e1.name = "Derived";
        e1.objectTypeId = "derived.object.type.1";
        e1.attributeNames.push_back("IntNumberWithoutLimit");
        e1.attributeNames.push_back("Derived Number Attribute");
        e1.entries.push_back({{"Derived Number Attribute", "1"}, {"IntNumberWithoutLimit", "9876"}});
        return e1;
    }

    LookupListDataList exampleDataList()
    {
        LookupListDataList result;
        result.push_back(bigColumnNumberExampleData());
        result.push_back(smallColumnNumberGenericExampleData());
        return result;
    }

    std::shared_ptr<LookupListEntriesModel> m_entriesModel;
    std::shared_ptr<LookupListEntriesSortFilterProxyModel> m_entriesProxyModel;
    std::shared_ptr<LookupListImportEntriesModel> m_importModel;
    QFile m_exportTestFile;
};

/**
 * Check export and building of import preview for simple LookupList with same size.
 */
TEST_F(LookupListTests, export_build_import_preview_lookup_list)
{
    for (auto& testData: exampleDataList())
        checkBuildingLookupListPreviewModel(testData, testData);
}
/**
 * Check export and building of import preview for empty LookupList.
 */
TEST_F(LookupListTests, export_build_import_preview_empty_lookup_list)
{
    LookupListData data = derivedObjectExampleData();
    data.entries.clear();

    givenLookupListEntriesModel(data, true);
    givenImportModel(data);
    whenBuildTablePreview(LookupListPreviewProcessor::EmptyFileError);
    thenRowCountIs(data.entries.size(), m_entriesModel.get());
}

TEST_F(LookupListTests, exported_data_is_valid)
{
    givenLookupListEntriesModel(bigColumnNumberExampleData());
    whenExportModelToFile();
    thenExportedDataIsTheSameAsInModel();
}

/**
 * Check that preview import table is building correctly for csv with lesser number of columns.
 */
TEST_F(LookupListTests, import_table_with_lesser_num_columns)
{
    checkBuildingLookupListPreviewModel(
        smallColumnNumberGenericExampleData(), bigColumnNumberExampleData());
}

/**
 * Check that preview import table is building correctly for csv with bigger number of columns.
 */
TEST_F(LookupListTests, import_table_with_bigger_num_columns)
{
    checkBuildingLookupListPreviewModel(
        bigColumnNumberExampleData(), smallColumnNumberGenericExampleData());
}

/**
 * Check that addition of empty rows is prohibited.
 */
TEST_F(LookupListTests, add_empty_row)
{
    const int initialRowCount = givenImportModel(bigColumnNumberExampleData());

    whenAddEntriesByImportModel(
        {{{"IntNumberWithoutLimit", ""}, {"Color", ""}, {"Approved", ""}}});

    // The entry should not be added to the model,
    // so the number of rows before and after must be the same.
    thenImportModelRowCountIs(initialRowCount);
}

/**
 * Check validation of generic list. Basically validation with disabled taxonomy.
 */
TEST_F(LookupListTests, validate_generic_list)
{
    StateView stateView({}, nullptr);
    const int initialRowCount =
        givenImportModel(smallColumnNumberGenericExampleData(), &stateView);

    // Create valid entry for Generic List and add it to the model.
    whenAddEntriesByImportModel({{{"Value", "trulala"}}});

    // Check that number of rows increased to one.
    thenImportModelRowCountIs(initialRowCount + 1);

    // Check that there are no fixup required.
    thenFixUpIsNotRequired();
}

/**
 * Check that revert of import deletes imported rows.
 */
TEST_F(LookupListTests, revert_import)
{
    StateView stateView({}, nullptr);
    const int initialRowCount =
        givenImportModel(smallColumnNumberGenericExampleData(), &stateView);

    // Create valid entry for Generic List.
    const int countOfAdditions = 12;

    // Adding the entry `countOfAdditions` times to model.
    whenAddEntriesByImportModel({countOfAdditions, {{"Value", "trulala"}}});

    // Check that there are no fixup required.
    thenFixUpIsNotRequired();

    // Check that number of rows incresead.
    thenImportModelRowCountIs(initialRowCount + countOfAdditions);

    whenRevertImport();

    // Check that number of rows is the same as before import.
    thenImportModelRowCountIs(initialRowCount);
}

/**
 * Check validation of lookup list with incorrect boolean value.
 */
TEST_F(LookupListTests, validate_bool_value)
{
    checkValidationOfEmptyRowWithOneIncorrect("Approved", "incorrect", "true");
    checkValidationOfRowsWithCorrectAndIncorrectValues("Approved");
}

/**
 * Check validation of lookup list with incorrect enum value.
 */
TEST_F(LookupListTests, validate_enum_value)
{
    checkValidationOfEmptyRowWithOneIncorrect("Enum", "incorrect", "base enum item 1.1");
    checkValidationOfRowsWithCorrectAndIncorrectValues("Enum");
}

/**
 * Check validation of lookup list with incorrect int number value, without limit.
 */
TEST_F(LookupListTests, validate_int_number_value_limitless)
{
    checkValidationOfEmptyRowWithOneIncorrect("IntNumberWithoutLimit", "incorrect", "1");
    checkValidationOfEmptyRowWithOneIncorrect("IntNumberWithoutLimit", "1.987", "1");
    checkValidationOfRowsWithCorrectAndIncorrectValues("IntNumberWithoutLimit");
}

/**
 * Check validation of lookup list with incorrect float number value, without limit.
 */
TEST_F(LookupListTests, validate_float_number_value_limitless)
{
    checkValidationOfEmptyRowWithOneIncorrect("FloatNumberWithoutLimit", "incorrect", "1.987");
    checkValidationOfEmptyRowWithOneIncorrect("FloatNumberWithoutLimit", "incorrect", "1");
    checkValidationOfRowsWithCorrectAndIncorrectValues("FloatNumberWithoutLimit");
}

/**
 * Check validation of lookup list with float number value, outside min/max range.
 */
TEST_F(LookupListTests, validate_float_number_value_with_limit)
{
    // Value less than min.
    checkValidationOfEmptyRowWithOneIncorrect("FloatNumberWithLimit", "0.1", "1.987");
    // Value more than max.
    checkValidationOfEmptyRowWithOneIncorrect("FloatNumberWithLimit", "11.1", "1.987");


    // Check type with limit only in min value. Value less than min.
    checkValidationOfEmptyRowWithOneIncorrect("FloatNumberWithMinLimit", "1.49", "19");

    // Check type with limit only in max value. Value more than max.
    checkValidationOfEmptyRowWithOneIncorrect("FloatNumberWithMaxLimit", "10.81", "19");

    checkValidationOfRowsWithCorrectAndIncorrectValues("FloatNumberWithLimit");
}

/**
 * Check validation of lookup list with int number value, outside min/max range.
 */
TEST_F(LookupListTests, validate_int_number_value_with_limit)
{
    // Value less than min.
    checkValidationOfEmptyRowWithOneIncorrect("IntNumberWithLimit", "-1", "19");
    // Value more than max.
    checkValidationOfEmptyRowWithOneIncorrect("IntNumberWithLimit", "60", "8");

    // Check type with limit only in min value. Value less than min.
    checkValidationOfEmptyRowWithOneIncorrect("IntNumberWithMinLimit", "-1", "19");

    // Check type with limit only in max value. Value more than max.
    checkValidationOfEmptyRowWithOneIncorrect("IntNumberWithMaxLimit", "134", "19");

    checkValidationOfRowsWithCorrectAndIncorrectValues("IntNumberWithLimit");
}

/**
 * Check validation of lookup list with negative int number value, outside min/max range.
 */
TEST_F(LookupListTests, validate_int_negative_number_value_with_limit)
{
    // Value less than min.
    checkValidationOfEmptyRowWithOneIncorrect("IntNumberWithLimitBellowZero", "-60", "-10");

    // Value more than max.
    checkValidationOfEmptyRowWithOneIncorrect("IntNumberWithLimitBellowZero", "100", "-5");

    checkValidationOfRowsWithCorrectAndIncorrectValues("IntNumberWithLimitBellowZero");
}

/**
 * Check validation of lookup list with incorrect color value.
 */
TEST_F(LookupListTests, validate_color_value)
{
    checkValidationOfEmptyRowWithOneIncorrect("Color", "incorrect", "blue");
    checkValidationOfEmptyRowWithOneIncorrect("Color", "incorrect", "base color 1.1");
    checkValidationOfRowsWithCorrectAndIncorrectValues("Color");
}

/**
 * Check hex RGB value is valid.
 */
TEST_F(LookupListTests, color_rgb_hex)
{
    auto data = bigColumnNumberExampleData();
    data.entries.push_back({{"Color", "blue"}});
    data.entries.push_back({{"Color", "green"}});
    givenLookupListEntriesModel(data);

    for (int row = 0; row < m_entriesModel->rowCount(); ++row)
    {
        for (int column = 0; column < m_entriesModel->columnCount(); ++column)
        {
            const auto result = whenRequestingColorHex(row, column);
            thenColorHexIsValidOrEmpty(row, column, result);
        }
    }
}

/**
 * Check hex RGB value is empty for incorrect or empty values.
 */
TEST_F(LookupListTests, color_rgb_hex_invalid_values)
{
    auto data = bigColumnNumberExampleData();
    data.entries.clear();
    data.entries.push_back({{"Color", "invalid1"}});
    data.entries.push_back({{"Color", "invalid3"}});
    data.entries.push_back({{"Color", ""}, {"Approved", "true"}});
    givenLookupListEntriesModel(data);

    for (int row = 0; row < m_entriesModel->rowCount(); ++row)
    {
        for (int column = 0; column < m_entriesModel->columnCount(); ++column)
        {
            const auto result = whenRequestingColorHex(row, column);
            thenColorHexIsEmpty(result);
        }
    }
}

/**
 * Check whether displayed color value is valid.
 */
TEST_F(LookupListTests, color_displayed_value)
{
    auto data = bigColumnNumberExampleData();
    data.entries.push_back({{"Color", "blue"}});
    data.entries.push_back({{"Color", "green"}});
    givenLookupListEntriesModel(data);

    for (int row = 0; row < m_entriesModel->rowCount(); ++row)
    {
        for (int column = 0; column < m_entriesModel->columnCount(); ++column)
        {
            const auto result = whenRequestingColorDisplayValue(row, column);
            thenColorNameIsValid(row, column, result);
        }
    }
}

/**
 * Check removing of incorrect entries, after reducing of attributes from Lookup List Model.
 * Removing of first column.
 */
TEST_F(LookupListTests, remove_first_column)
{
    checkRemovingOfAttribute(bigColumnNumberExampleData().attributeNames.front());
}

/**
 * Check removing of incorrect entries, after reducing of attributes from Lookup List Model
 * Removing of last column.
 */
TEST_F(LookupListTests, remove_last_column)
{
    checkRemovingOfAttribute(bigColumnNumberExampleData().attributeNames.back());
}

/**
 * Check removing of incorrect entries, after reducing of attributes from Lookup List Model
 * Removing column in the middle.
 */
TEST_F(LookupListTests, remove_middle_column)
{
    const auto data = bigColumnNumberExampleData();
    const auto columnToDelete = data.attributeNames[data.attributeNames.size() / 2];
    checkRemovingOfAttribute(columnToDelete);
}

/**
 * Check validation of attribute in derived Object.
 */
TEST_F(LookupListTests, derived_type_as_list_type)
{
    const auto data = derivedObjectExampleData();
    checkValidationOfEmptyRowWithOneIncorrect("Derived Number Attribute", "incorrect", "1", data);
}

/**
 * Check setting of different LookupListModel to same LookupListEntriesModel
 */
TEST_F(LookupListTests, change_lookup_list)
{
    LookupListModel initialGenericLookupList(smallColumnNumberGenericExampleData());
    const auto initialLookupListModelData = initialGenericLookupList.rawData();

    givenLookupListEntriesModel(&initialGenericLookupList);

    // Change LookupListModel to one with attributes.
    whenSetNewLookupListModel(bigColumnNumberExampleData());

    // Check that LookupListData of LookupListEntriesModel have changed.
    ASSERT_NE(initialLookupListModelData, m_entriesModel->listModel()->rawData());

    const int rowCountBeforeChange = m_entriesModel->rowCount();
    // Add new correct entry.
    whenAddEntries({correctRow()});

    // Check that row was added successfuly.
    thenRowCountIs(rowCountBeforeChange + 1, m_entriesModel.get());

    // Remove one column from new model.
    whenRemoveColumn();

    // Check that previous Lookup List Model didn't change.
    ASSERT_EQ(initialLookupListModelData, initialGenericLookupList.rawData());
}

/**
 * Check search of values in Lookup List Entries Model is case independent
 */
TEST_F(LookupListTests, search_is_case_independent)
{
    LookupListData testData;
    testData.id = nx::Uuid::createUuid();
    testData.name = "Values";
    testData.attributeNames.push_back("Value");
    testData.entries = {{{"Value", "aaa"}},
        {{"Value", "Aaa"}},
        {{"Value", "bbb"}},
        {{"Value", "bbbaa"}},
        {{"Value", "AAAA"}},
        {{"Value", "AaaA"}},
        {{"Value", "6"}}};

    givenLookupListEntriesProxyModel(testData);

    whenSetFilter("aa");
    const int expectedRowCount = 5;
    thenRowCountIs(expectedRowCount, m_entriesProxyModel.get());

    whenSetFilter("AA");
    thenRowCountIs(expectedRowCount, m_entriesProxyModel.get());
}

/**
 * Check search of values in Lookup List Entries Model is performed on displayed values
 */

TEST_F(LookupListTests, search_on_displayed_values)
{
    givenLookupListEntriesProxyModel(bigColumnNumberExampleData());

    whenSetFilter("red");
    thenRowCountIs(1, m_entriesProxyModel.get());

    whenSetFilter("No");
    thenRowCountIs(1, m_entriesProxyModel.get());

    whenSetFilter("Yes");
    thenRowCountIs(1, m_entriesProxyModel.get());

    whenSetFilter("blue");
    thenRowCountIs(1, m_entriesProxyModel.get());

    whenSetFilter("green");
    thenRowCountIs(1, m_entriesProxyModel.get());
}

TEST_F(LookupListTests, search_by_all_columns)
{
    givenLookupListEntriesProxyModel(bigColumnNumberExampleData());

    // Search by all values in first row.
    for (int i = 0; i < m_entriesModel->columnCount(); ++i)
    {
        const auto modelIndex = m_entriesModel->index(0, i);
        ASSERT_TRUE(modelIndex.isValid());
        whenSetFilter(modelIndex.data().toString());
        thenRowCountIs(1, m_entriesProxyModel.get());
    }
}

TEST_F(LookupListTests, rename_generic_list_column)
{
    givenLookupListEntriesModel(smallColumnNumberGenericExampleData());
    const QList<QString> newColumn{"New one"};
    whenSetAttributesNames(newColumn);
    thenListEntriesHasAttributes(newColumn);
}

TEST_F(LookupListTests, sorting_of_numeric_values)
{
    givenLookupListEntriesProxyModel(numericColumnExampleData());
    whenSortingByColumn(0, Qt::AscendingOrder);
    thenEmptyValuesAreAtTheEnd(0);
    thenValuesAreOrdered<int>(Qt::AscendingOrder, 0);

    whenSortingByColumn(0, Qt::DescendingOrder);
    thenEmptyValuesAreAtTheBegining(0);
    thenValuesAreOrdered<int>(Qt::DescendingOrder, 0);
}

TEST_F(LookupListTests, sorting_of_string_values)
{
    givenLookupListEntriesProxyModel(stringColumnExampleData());
    whenSortingByColumn(0, Qt::AscendingOrder);
    thenEmptyValuesAreAtTheEnd(0);
    thenValuesAreOrdered<QString>(Qt::AscendingOrder, 0);

    whenSortingByColumn(0, Qt::DescendingOrder);
    thenEmptyValuesAreAtTheBegining(0);
    thenValuesAreOrdered<QString>(Qt::DescendingOrder, 0);
}

TEST_F(LookupListTests, check_column_header_after_revert_import)
{
    const auto data = bigColumnNumberExampleData();
    givenLookupListEntriesModel(data);
    givenImportModel(data);

    whenBuildTablePreview();
    // The default selected choice is the same as attributeNames.
    thenBuildPreviewHeaderEqualTo(data.attributeNames);

    auto newSelectedChoice = data.attributeNames;
    std::reverse(newSelectedChoice.begin(), newSelectedChoice.end());

    whenSetAttributesPosition(newSelectedChoice);
    thenBuildPreviewHeaderEqualTo(newSelectedChoice);

    std::vector<QVariantMap> entriesToAdd;
    std::map<QString, QString> incorrectToCorrect;
    givenRowsWithAllCorrectValuesExceptOne(
            entriesToAdd, incorrectToCorrect, "Color", 5);

    // Import with incorrect values.
    whenAddEntriesByImportModel(entriesToAdd);
    thenFixUpIsRequired();

    whenRevertImport();
    thenBuildPreviewHeaderEqualTo(newSelectedChoice);
}

TEST_F(LookupListTests, check_column_header_without_reset)
{
    const auto data = bigColumnNumberExampleData();
    givenLookupListEntriesModel(data);
    givenImportModel(data);

    whenBuildTablePreview();
    thenBuildPreviewHeaderEqualTo(data.attributeNames);

    auto newSelectedChoice = data.attributeNames;
    std::reverse(newSelectedChoice.begin(), newSelectedChoice.end());

    whenSetAttributesPosition(newSelectedChoice);
    thenBuildPreviewHeaderEqualTo(newSelectedChoice);

    whenBuildTablePreview(LookupListPreviewProcessor::Success, false);
    thenBuildPreviewHeaderEqualTo(newSelectedChoice);
}

} // namespace nx::vms::client::desktop
