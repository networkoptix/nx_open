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

    void givenLookupListEntriesModel(const LookupListData& data)
    {
        m_entriesModel = std::make_shared<LookupListEntriesModel>();
        auto lookupList = new LookupListModel(data, m_entriesModel.get());
        m_entriesModel->setListModel(lookupList);
        m_entriesModel->setTaxonomy(stateView());
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

    void whenBuildTablePreview()
    {
        whenExportModelToFile();

        LookupListPreviewProcessor previewProcessor;
        ASSERT_TRUE(previewProcessor.buildTablePreview(
            m_importModel.get(), m_exportTestFile.fileName(), ",", true));
        m_exportTestFile.remove();
    }

    void whenRevertImport() { m_importModel->revertImport(); }

    void whenSetFilter(const QString& filter)
    {
        m_entriesProxyModel->setFilterRegularExpression(filter);
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
        entry["Number"] = "1";
        entry["Color"] = "#FF0000";
        entry["Enum"] = "base enum item 2.1";
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

    LookupListData bigColumnNumberExampleData()
    {
        LookupListData e1;
        e1.objectTypeId = "base.object.type.1";
        e1.id = nx::Uuid::createUuid();
        e1.name = "Numbers";
        e1.attributeNames.push_back("Number");
        e1.attributeNames.push_back("Color");
        e1.attributeNames.push_back("Enum");
        e1.attributeNames.push_back("Approved");
        e1.entries = {
            {{"Number", "12345"},
                {"Color", "#FF0000"},
                {"Enum", "base enum item 2.1"},
                {"Approved", "true"}},
            {{"Number", "67890"}, {"Approved", "false"}},
            {{"Color", "#0000FF"}, {"Number", "9876"}},
            {{"Enum", "base enum item 2.1"}, {"Color", "#0000FF"}},
        };
        return e1;
    }

    LookupListData derivedObjectExampleData()
    {
        LookupListData e1;
        e1.id = nx::Uuid::createUuid();
        e1.name = "Derived";
        e1.objectTypeId = "derived.object.type.1";
        e1.attributeNames.push_back("Number");
        e1.attributeNames.push_back("Derived Number Attribute");
        e1.entries.push_back({{"Derived Number Attribute", "1"}, {"Number", "9876"}});
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

    whenAddEntriesByImportModel({{{"Number", ""}, {"Color", ""}, {"Approved", ""}}});

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
 * Check validation of lookup list with incorrect number value.
 */
TEST_F(LookupListTests, validate_number_value)
{
    checkValidationOfEmptyRowWithOneIncorrect("Number", "incorrect", "1");
    checkValidationOfRowsWithCorrectAndIncorrectValues("Number");
}

/**
 * Check validation of lookup list with incorrect color value.
 */
TEST_F(LookupListTests, validate_color_value)
{
    checkValidationOfEmptyRowWithOneIncorrect("Color", "incorrect", "#000000");
    checkValidationOfEmptyRowWithOneIncorrect("Color", "incorrect", "blue");
    checkValidationOfEmptyRowWithOneIncorrect("Color", "incorrect", "base color 1.1");
    checkValidationOfRowsWithCorrectAndIncorrectValues("Color");
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
    thenRowCountIs(2, m_entriesProxyModel.get());
}

} // namespace nx::vms::client::desktop
