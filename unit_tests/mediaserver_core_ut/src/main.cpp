#include <QtCore>

#include <nx/network/test_support/run_test.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include <test_support/utils.h>
#include <common/static_common_module.h>

namespace {

std::unique_ptr<QCommandLineParser> fillConfig(const QStringList& arguments)
{
    auto parser = std::make_unique<QCommandLineParser>();
    parser->addOptions({ {{"t", "tmp"}, "Temporary working directory path. Default: 'tmp'", "tmp"},
                       {"ftp-storage-url", "Ftp storage url"},
                       {"smb-storage-url", "Smb storage url"} });
    parser->addHelpOption();
    parser->parse(arguments);

    if (parser->isSet("tmp") && nx::ut::utils::validateAndOrCreatePath(parser->value("tmp")))
        nx::ut::cfg::configInstance().tmpDir = parser->value("tmp");
    else
        nx::ut::cfg::configInstance().tmpDir = QDir(QDir::tempPath()).absoluteFilePath("nx_ut");

    if (parser->isSet("ftp-storage-url"))
        nx::ut::cfg::configInstance().ftpUrl = parser->value("ftp-storage-url");
    if (parser->isSet("smb-storage-url"))
        nx::ut::cfg::configInstance().smbUrl = parser->value("smb-storage-url");

    return parser;
}

std::unique_ptr<QCommandLineParser> fillConfig(QCoreApplication& app)
{
    return fillConfig(app.arguments());
}

} // namespace

int main(int argc, char** argv)
{
    QnStaticCommonModule staticCommonModule(nx::vms::api::PeerType::server);

#ifndef ENABLE_CLOUD_TEST
    QCoreApplication app(argc, argv);
    auto parser = fillConfig(app);
#else
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments.push_back(QString::fromUtf8(argv[i]));
    auto parser = fillConfig(arguments);
#endif
    int result = nx::network::test::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser& /*args*/)
        {
            return nx::utils::test::DeinitFunctions();
        });
    if (parser->isSet("help"))
        parser->showHelp();
    return result;
}
