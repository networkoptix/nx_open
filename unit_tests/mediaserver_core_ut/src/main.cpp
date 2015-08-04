#define GTEST_HAS_POSIX_RE 0
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

    QCommandLineOption smbUrlOption(
        QStringList() << "S" << "smb-storage-url",
        QCoreApplication::translate("main", "SMB storage URL."),
        QCoreApplication::translate("main", "URL")
    );
    parser.addOption(smbUrlOption);

    parser.process(app);

    tg.prepare(
        parser.value(
            ftpUrlOption
        ),
        parser.value(
            smbUrlOption
        )
    );

    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    return result;
}
