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

LookupListDataList exampleDataList()
{
    LookupListDataList result;

    LookupListData e1;
    e1.id = QnUuid::createUuid();
    e1.name = "Numbers";
    e1.attributeNames.push_back("Number");
    e1.attributeNames.push_back("Color");
    e1.entries = {
        {{"Number", "12345"}, {"Color", "red"}}, {{"Number", "67890"}}, {{"Color", "blue"}}};
    result.push_back(e1);
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

    result.push_back(e2);

    return result;
}

} // namespace

/*
 * Check export and building of import preview for simple LookupList
 */
TEST(LookupListTests, export_build_import_preview_lookup_list)
{
    for (auto& testModel: exampleDataList())
    {
        LookupListModel modelList(testModel);
        LookupListEntriesModel model;
        model.setListModel(&modelList);
        QFile exportTestFile(
            nx::utils::TestOptions::temporaryDirectoryPath(true) + "/" + testModel.name + ".csv");

        ASSERT_TRUE(exportTestFile.open(QFile::WriteOnly));

        // Export LookupListEntriesModel.
        QTextStream exportStream(&exportTestFile);
        model.exportEntries({}, exportStream);
        exportStream.flush();
        ASSERT_NE(exportTestFile.size(), 0);
        exportTestFile.close();

        // Build LookupPreviewEntriesModel for import preview.
        LookupPreviewEntriesModel previewModel;
        LookupListPreviewProcessor previewProcessor;
        ASSERT_TRUE(previewProcessor.buildTablePreview(
            &previewModel, exportTestFile.fileName(), ",", true));

        // Check that all rows from original model are presented in import preview.
        ASSERT_EQ(previewModel.rowCount(), testModel.entries.size());

        // Cleanup.
        exportTestFile.remove();
    }
}
