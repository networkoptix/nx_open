#include <QtCore>

#include <nx/network/test_support/run_test.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include <test_support/utils.h>

nx::ut::cfg::Config config;

static void fillConfig(const QStringList& arguments)
{
    QCommandLineParser parser;
    parser.addOptions({ {{"t", "tmp"}, "Temporary working directory path. Default: 'tmp'", "tmp"},
                       {"ftp-storage-url", "Ftp storage url"},
                       {"smb-storage-url", "Smb storage url"} });
    parser.addHelpOption();
    parser.parse(arguments);

    if (parser.isSet("help"))
    {
        parser.showHelp();
        QCoreApplication::exit(0);
        return;
    }

    if (parser.isSet("tmp") && nx::ut::utils::validateAndOrCreatePath(parser.value("tmp")))
        config.tmpDir = parser.value("tmp");
    else
        config.tmpDir = QDir(QDir::tempPath()).absoluteFilePath("nx_ut");

    if (parser.isSet("ftp-storage-url"))
        config.ftpUrl = parser.value("ftp-storage-url");
    if (parser.isSet("smb-storage-url"))
        config.smbUrl = parser.value("smb-storage-url");
}

static void fillConfig(QCoreApplication& app)
{
    fillConfig(app.arguments());
}

int main(int argc, char** argv)
{
#ifndef ENABLE_CLOUD_TEST
    QCoreApplication app(argc, argv);
    fillConfig(app);
#else
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments.push_back(QString::fromUtf8(argv[i]));
    fillConfig(arguments);
#endif
    return nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& /*args*/)
        {
            return nx::utils::test::DeinitFunctions();
        },
        0);
}
