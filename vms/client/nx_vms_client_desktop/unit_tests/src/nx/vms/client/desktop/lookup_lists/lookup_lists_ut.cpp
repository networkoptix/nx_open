// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QFileInfo>
#include <QString>

#include <gtest/gtest.h>

#include <nx/utils/test_support/test_options.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/analytics/taxonomy/object_type.h>
#include <nx/vms/client/desktop/analytics/taxonomy/state_view.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_export_processor.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_model.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_preview_processor.h>

#include "taxonomy_based_ut.h"

using namespace nx::vms::client::desktop::analytics::taxonomy;
using namespace nx::vms::api;
namespace nx::vms::client::desktop {
class LookupListTests: public TaxonomyBasedTest
{
public:
    LookupListData smallColumnNumberExampleData()
    {
        LookupListData e2;
        e2.id = QnUuid::createUuid();
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
        e1.id = QnUuid::createUuid();
        e1.name = "Numbers";
        e1.attributeNames.push_back("Number");
        e1.attributeNames.push_back("Color");
        e1.attributeNames.push_back("Enum");
        e1.attributeNames.push_back("Approved");
        e1.entries = {
            {{"Number", "12345"},
                {"Color", "red"},
                {"Enum", "base enum item 2.1"},
                {"Approved", "true"}},
            {{"Number", "67890"},{"Approved", "false"}},
            {{"Number", "67890"}},
            {{"Color", "blue"}},
        };
        return e1;
    }

    LookupListDataList exampleDataList()
    {
        LookupListDataList result;
        result.push_back(bigColumnNumberExampleData());
        result.push_back(smallColumnNumberExampleData());
        return result;
    }

    bool exportModelToFile(QFile& exportTestFile, LookupListEntriesModel& entriesModel)
    {
        if (!exportTestFile.open(QFile::WriteOnly))
            return false;

        // Export LookupListEntriesModel.
        QTextStream exportStream(&exportTestFile);
        entriesModel.exportEntries({}, exportStream);
        exportStream.flush();

        if (exportTestFile.size() == 0)
            return false;

        exportTestFile.close();

        return true;
    }

    void checkBuildingLookupListPreviewModel(
        const LookupListData& dataToImport, const LookupListData& modelImportTo)
    {
        QFile exportTestFile(nx::utils::TestOptions::temporaryDirectoryPath(true) + "/"
            + dataToImport.name + ".csv");
        LookupListModel exportedModel(dataToImport);
        LookupListEntriesModel exportedEntriesModel;
        exportedEntriesModel.setListModel(&exportedModel);

        ASSERT_TRUE(exportModelToFile(exportTestFile, exportedEntriesModel));

        LookupListModel lookupListModelImportTo(modelImportTo);
        LookupListEntriesModel listEntriesImportTo;
        listEntriesImportTo.setListModel(&lookupListModelImportTo);
        LookupListImportEntriesModel previewModel;

        previewModel.setLookupListEntriesModel(&listEntriesImportTo);
        LookupListPreviewProcessor previewProcessor;
        ASSERT_TRUE(previewProcessor.buildTablePreview(
            &previewModel, exportTestFile.fileName(), ",", true));

        // Check that preview model contains the same amount of rows as in exported small model.
        ASSERT_EQ(previewModel.rowCount(), dataToImport.entries.size());

        // Check that preview model contains the same amount of columns as exported model.
        ASSERT_EQ(previewModel.columnCount(), dataToImport.attributeNames.size());

        for (int c = 0; c < previewModel.columnCount(); ++c)
        {
            auto curHeader = previewModel.headerData(c, Qt::Horizontal);

            // Check that attribute name, by position i in LookupListModel
            // is on first position of i column
            // OR "Do not import" text on additional columns.
            ASSERT_TRUE(curHeader.isValid());
            ASSERT_FALSE(curHeader.toList().empty());
            ASSERT_EQ(curHeader.toList().front().toString(),
                c < modelImportTo.attributeNames.size() ? modelImportTo.attributeNames[c]
                                                        : previewModel.doNotImportText());

            for (int r = 0; r < dataToImport.entries.size(); ++r)
            {
                // Check that data in previewModel is the same as in imported file.
                ASSERT_EQ(
                    previewModel.index(r, c).data(), exportedEntriesModel.index(r, c).data());
            }
        }
        exportTestFile.remove();
    }

    std::shared_ptr<LookupListImportEntriesModel> prepareImportModel(
        const LookupListData& data, StateView* taxonomy = nullptr)
    {
        auto importModel = std::make_shared<LookupListImportEntriesModel>();
        auto lookupList = new LookupListModel(data, importModel.get());
        auto entriesModel = new LookupListEntriesModel(importModel.get());
        entriesModel->setListModel(lookupList);

        entriesModel->setTaxonomy(taxonomy ? taxonomy : stateView());
        importModel->setLookupListEntriesModel(entriesModel);
        return importModel;
    }

    void checkValidationOfEmptyRowWithOneIncorrect(
        const QString& attributeName, const QString& incorrect, const QString& correct)
    {
        auto importModel = prepareImportModel(bigColumnNumberExampleData());
        const int rowCountBefore = importModel->lookupListEntriesModel()->rowCount();

        // Create invalid entry for Generic List and attempt to add it to the model.
        QVariantMap entry;
        entry[attributeName] = incorrect;
        importModel->addLookupListEntry(entry);

        // Check that number of rows didn't change
        ASSERT_EQ(rowCountBefore, importModel->lookupListEntriesModel()->rowCount());

        // Check fixup is required.
        ASSERT_TRUE(importModel->fixupRequired());

        // Apply fix for incorrect attribute.
        importModel->addFixForWord(attributeName, incorrect, correct);
        importModel->applyFix();
        // Check that number of rows increased to one
        ASSERT_EQ(rowCountBefore + 1, importModel->lookupListEntriesModel()->rowCount());
    }

    QVariantMap correctRow()
    {
        QVariantMap entry;
        entry["Approved"] = "true";
        entry["Number"] = "1";
        entry["Color"] = "red";
        entry["Enum"] = "base enum item 2.1";
        return entry;
    }
    void checkValidationOfRowsWithCorrectAndIncorrectValues(
        const QString& attributeNameOfIncorrectVal,
        const std::vector<QVariantMap>& entriesToAdd,
        const std::map<QString, QString>& incorrectToCorrect)
    {
        auto importModel = prepareImportModel(bigColumnNumberExampleData());
        const int rowCountBefore = importModel->lookupListEntriesModel()->rowCount();

        // Attempt to add entry to the model
        for (auto entry: entriesToAdd)
            importModel->addLookupListEntry(entry);

        // Check that number of rows increased, since there are valid values in row
        const int newNumberOfRows = rowCountBefore + entriesToAdd.size();
        ASSERT_EQ(newNumberOfRows, importModel->lookupListEntriesModel()->rowCount());

        // Check fixup is required.
        ASSERT_TRUE(importModel->fixupRequired());

        // Apply fixes for incorrect values
        for (auto& [incorrectWord, fix]: incorrectToCorrect)
            importModel->addFixForWord(attributeNameOfIncorrectVal, incorrectWord, fix);

        importModel->applyFix();

        // Check that number of rows didn't change
        ASSERT_EQ(newNumberOfRows, importModel->lookupListEntriesModel()->rowCount());
    }
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
 * Check that preview import table is building correctly for csv with lesser number of columns.
 */
TEST_F(LookupListTests, import_table_with_lesser_num_columns)
{
    checkBuildingLookupListPreviewModel(
        smallColumnNumberExampleData(), bigColumnNumberExampleData());
}

/**
 * Check that preview import table is building correctly for csv with bigger number of columns.
 */
TEST_F(LookupListTests, import_table_with_bigger_num_columns)
{
    checkBuildingLookupListPreviewModel(
        bigColumnNumberExampleData(), smallColumnNumberExampleData());
}

/**
 * Check that addition of empty rows is prohibited.
 */
TEST_F(LookupListTests, add_empty_row)
{
    // Initalize LookupListImportEntriesModel with required objects.
    LookupListModel exportedModel(bigColumnNumberExampleData());
    LookupListEntriesModel entriesModel;
    entriesModel.setListModel(&exportedModel);
    LookupListImportEntriesModel importModel;
    importModel.setLookupListEntriesModel(&entriesModel);

    // Create entry with empty values of columns and attempt to add it to the model.
    QVariantMap entry;
    entry["Number"] = {};
    entry["Color"] = {};

    const int rowCountBefore = importModel.lookupListEntriesModel()->rowCount();
    importModel.addLookupListEntry(entry);
    const int rowCountAfter = importModel.lookupListEntriesModel()->rowCount();

    // The entry should not be added to the model,
    // so the number of rows before and after must be the same.
    ASSERT_EQ(rowCountBefore, rowCountAfter);
}

/**
 * Check validation of generic list. Basically validation with disabled taxonomy.
 */
TEST_F(LookupListTests, validate_generic_list)
{
    StateView stateView({}, nullptr);
    auto importModel = prepareImportModel(smallColumnNumberExampleData(), &stateView);

    const int rowCountBefore = importModel->lookupListEntriesModel()->rowCount();

    // Create valid entry for Generic List and add it to the model.
    QVariantMap entry;
    entry["Value"] = "trulala";
    importModel->addLookupListEntry(entry);

    // Check that number of rows increased to one.
    ASSERT_EQ(rowCountBefore + 1, importModel->lookupListEntriesModel()->rowCount());

    // Check that there are no fixup required.
    ASSERT_FALSE(importModel->fixupRequired());
}

/**
 * Check that revert of import deletes imported rows.
 */
TEST_F(LookupListTests, revert_import)
{
    StateView stateView({}, nullptr);
    auto importModel = prepareImportModel(smallColumnNumberExampleData(), &stateView);

    // Create valid entry for Generic List.
    QVariantMap entry;
    entry["Value"] = "trulala";

    const int countOfAdditions = 12;
    const int rowCountBefore = importModel->lookupListEntriesModel()->rowCount();
    // Adding the entry `countOfAdditions` times to model.
    for (int i = 0; i < countOfAdditions; i++)
        importModel->addLookupListEntry(entry);

    // Check that there are no fixup required.
    ASSERT_FALSE(importModel->fixupRequired());

    // Check that number of rows incresead.
    ASSERT_EQ(
        rowCountBefore + countOfAdditions, importModel->lookupListEntriesModel()->rowCount());

    importModel->revertImport();

    // Check that number of rows is the same as before import.
    ASSERT_EQ(rowCountBefore, importModel->lookupListEntriesModel()->rowCount());
}

/**
 * Check validation of lookup list with incorrect boolean value.
 */
TEST_F(LookupListTests, validate_bool_value)
{
    checkValidationOfEmptyRowWithOneIncorrect("Approved", "incorrect", "true");

    // Prepare rows with all correct values, except one
    std::vector<QVariantMap> rows;
    std::map<QString, QString> fixes;
    const int numberOfRows = 5;
    for (int i = 0; i < numberOfRows; i++)
    {
        QVariantMap entry = correctRow();
        const QString incorrectVal = QString::number(i);
        entry["Approved"] = incorrectVal;
        fixes[incorrectVal] = i % 2 == 0 ? "true" : "false";
        rows.push_back(entry);
    }
    checkValidationOfRowsWithCorrectAndIncorrectValues("Approved", rows, fixes);
}

/**
 * Check validation of lookup list with incorrect enum value.
 */
TEST_F(LookupListTests, validate_enum_value)
{
    checkValidationOfEmptyRowWithOneIncorrect("Enum", "incorrect", "base enum item 1.1");

    // Prepare rows with all correct values, except one
    std::vector<QVariantMap> rows;
    std::map<QString, QString> fixes;
    const int numberOfRows = 5;
    for (int i = 0; i < numberOfRows; i++)
    {
        QVariantMap entry = correctRow();
        const QString incorrectVal = QString::number(i);
        entry["Enum"] = incorrectVal;
        fixes[incorrectVal] = "base enum item 1.2";
        rows.push_back(entry);
    }
    checkValidationOfRowsWithCorrectAndIncorrectValues("Enum", rows, fixes);
}

/**
 * Check validation of lookup list with incorrect number value.
 */
TEST_F(LookupListTests, validate_number_value)
{
    checkValidationOfEmptyRowWithOneIncorrect("Number", "incorrect", "1");

    // Prepare rows with all correct values, except one
    std::vector<QVariantMap> rows;
    std::map<QString, QString> fixes;
    const int numberOfRows = 5;
    for (int i = 0; i < numberOfRows; i++)
    {
        QVariantMap entry = correctRow();
        const QString incorrectVal = "incorrect_" + QString::number(i);
        entry["Number"] = incorrectVal;
        fixes[incorrectVal] = QString::number(i);
        rows.push_back(entry);
    }
    checkValidationOfRowsWithCorrectAndIncorrectValues("Number", rows, fixes);
}

/**
 * Check validation of lookup list with incorrect color value.
 */
TEST_F(LookupListTests, validate_color_value)
{
    checkValidationOfEmptyRowWithOneIncorrect("Color", "incorrect", "#000000");

    // Prepare rows with all correct values, except one
    std::vector<QVariantMap> rows;
    std::map<QString, QString> fixes;
    const int numberOfRows = 5;
    for (int i = 0; i < numberOfRows; i++)
    {
        QVariantMap entry = correctRow();
        const QString incorrectVal = "incorrect_" + QString::number(i);
        entry["Color"] = incorrectVal;
        fixes[incorrectVal] = "#000000";
        rows.push_back(entry);
    }
    checkValidationOfRowsWithCorrectAndIncorrectValues("Color", rows, fixes);
}

} // namespace nx::vms::client::desktop
