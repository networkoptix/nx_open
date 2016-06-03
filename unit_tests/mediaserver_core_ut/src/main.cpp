#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>
#include <QtCore>

#include "utils.h"
#include <nx/network/socket_global.h>

nx::ut::cfg::Config config;

namespace
{

void fillConfig(QCoreApplication &app)
{
    QCommandLineParser parser;
    parser.addOptions({{{"t", "tmp"}, "Temporary working directory path.", "tmp"},
                       {"ftp-storage-url", "Ftp storage url"},
                       {"smb-storage-url", "Smb storage url"} });
    parser.addHelpOption();
    parser.parse(app.arguments());

    if (parser.isSet("help"))
    {
        parser.showHelp();
        QCoreApplication::exit(0);
    }

    if (parser.isSet("tmp") && nx::ut::utils::validateAndOrCreatePath(parser.value("tmp")))
        config.tmpDir = parser.value("tmp");
    else
        config.tmpDir = "tmp";

    if (parser.isSet("ftp-storage-url"))
        config.ftpUrl = parser.value("ftp-storage-url");
    if (parser.isSet("smb-storage-url"))
        config.smbUrl = parser.value("smb-storage-url");

}

} // anonymous namespace

int main(int argc, char **argv)
{
	nx::network::SocketGlobals::InitGuard sgGuard;
    QCoreApplication app(argc, argv);
    fillConfig(app);

    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    return result;
}
