// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QFileInfo>
#include <QString>

#include <gtest/gtest.h>

#include <nx/utils/test_support/test_options.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_export_processor.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_model.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_list_preview_processor.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::api;

namespace {

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
    e1.id = QnUuid::createUuid();
    e1.name = "Numbers";
    e1.attributeNames.push_back("Number");
    e1.attributeNames.push_back("Color");
    e1.attributeNames.push_back("Something Cool");
    e1.attributeNames.push_back("License.Plate");
    e1.entries = {{{"Number", "12345"}, {"Color", "red"}, {"Something Cool", ""}},
        {{"Number", "67890"}, {"License.Plate","1234"}},
        {{"Number", "67890"}},
        {{"Color", "blue"}},};
    return e1;
}

LookupListDataList exampleDataList()
{
    LookupListDataList result;
    result.push_back(bigColumnNumberExampleData());
    result.push_back(smallColumnNumberExampleData());
    return result;
}

bool exportModelToFile(
    QFile& exportTestFile, LookupListEntriesModel& entriesModel)
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

} // namespace

void checkBuildingLookupListPreviewModel(
    const LookupListData& dataToImport, const LookupListData& modelImportTo)
{
    QFile exportTestFile(
        nx::utils::TestOptions::temporaryDirectoryPath(true) + "/" + dataToImport.name + ".csv");
    LookupListModel exportedModel(dataToImport);
    LookupListEntriesModel exportedEntriesModel;
    exportedEntriesModel.setListModel(&exportedModel);

    ASSERT_TRUE(exportModelToFile(exportTestFile, exportedEntriesModel));

    LookupListModel lookupListModelImportTo(modelImportTo);
    LookupListEntriesModel listEntriesImportTo;
    listEntriesImportTo.setListModel(&lookupListModelImportTo);
    LookupListPreviewEntriesModel previewModel;

    previewModel.setLookupListEntriesModel(&listEntriesImportTo);
    LookupListPreviewProcessor previewProcessor;
    ASSERT_TRUE(
        previewProcessor.buildTablePreview(&previewModel, exportTestFile.fileName(), ",", true));

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
            ASSERT_EQ(previewModel.index(r, c).data(), exportedEntriesModel.index(r, c).data());
        }
    }
    exportTestFile.remove();
}

/*
 * Check export and building of import preview for simple LookupList with same size.
 */
TEST(LookupListTests, export_build_import_preview_lookup_list)
{
    for (auto& testData: exampleDataList())
        checkBuildingLookupListPreviewModel(testData,testData);
}

/*
 * Check that preview import table is building correctly for csv with lesser number of columns.
 */
TEST(LookupListTests, import_table_with_lesser_num_columns)
{
    checkBuildingLookupListPreviewModel(
        smallColumnNumberExampleData(), bigColumnNumberExampleData());
}

/*
 * Check that preview import table is building correctly for csv with bigger number of columns.
 */
TEST(LookupListTests, import_table_with_bigger_num_columns)
{
    checkBuildingLookupListPreviewModel(
        bigColumnNumberExampleData(), smallColumnNumberExampleData());
}
