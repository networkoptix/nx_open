#include <QtCore>

#include <nx/network/test_support/run_test.h>

#include <test_support/utils.h>
#include <common/static_common_module.h>
#include <iostream>

namespace {

std::unique_ptr<QCommandLineParser> fillConfig(const QStringList& arguments)
{
    auto parser = std::make_unique<QCommandLineParser>();
    parser->addOptions({ {{"t", "tmp"}, "Temporary working directory path. Default: 'tmp'", "tmp"},
                       {"ftp-storage-url", "Ftp storage url"},
                       {"smb-storage-url", "Smb storage url"},
                       {"enable-discovery", "Enable discovery"}});
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

    nx::ut::cfg::configInstance().enableDiscovery = parser->isSet("enable-discovery");

    return parser;
}

std::unique_ptr<QCommandLineParser> fillConfig(QCoreApplication& app)
{
    return fillConfig(app.arguments());
}

} // namespace

int main(int argc, char** argv)
{
    #ifndef ENABLE_CLOUD_TEST
        QCoreApplication app(argc, argv);
        auto parser = fillConfig(app);
    #else
        QStringList arguments;
        for (int i = 0; i < argc; ++i)
            arguments.push_back(QString::fromUtf8(argv[i]));
        auto parser = fillConfig(arguments);
    #endif

    std::unique_ptr<QnStaticCommonModule> staticCommonModule;
    int result = nx::network::test::runTest(
        argc, argv,
        [&staticCommonModule](const nx::utils::ArgumentParser& /*args*/)
        {
            staticCommonModule = std::make_unique<QnStaticCommonModule>(
                nx::vms::api::PeerType::server);

            return nx::utils::test::DeinitFunctions();
        });
    if (parser->isSet("help"))
        std::cout << parser->helpText().toUtf8();
    return result;
}
