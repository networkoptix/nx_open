#include <gtest/gtest.h>
#include <qcoreapplication.h>
#include <QCommandLineParser>

#include "storage/abstract_storage_resource.h"

extern test::StorageTestGlobals tg;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("Test helper");
    
    QCommandLineOption ftpUrlOption(
        QStringList() << "F" << "ftp-storage-url",
        QCoreApplication::translate("main", "Ftp storage URL."),
        QCoreApplication::translate("main", "URL")
    );
    parser.addOption(ftpUrlOption);
    parser.process(app);

    tg.prepare(parser.value(ftpUrlOption));

    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    return result;
}
