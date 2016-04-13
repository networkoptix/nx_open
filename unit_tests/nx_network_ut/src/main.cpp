/**********************************************************
* Dec 29, 2015
* akolesnikov
***********************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/ssl_socket.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <utils/common/ssl_gen_cert.h>
#include <utils/common/command_line_parser.h>

bool readCmdArguments(int argc, char **argv);

int main(int  argc, char **argv)
{
    nx::network::SocketGlobals::InitGuard sgGuard;
    ::testing::InitGoogleMock(&argc, argv);

    if (!readCmdArguments(argc, argv))
        return false;

    const auto sslCert = tmpnam(nullptr);
    if (const auto ret = generateSslCertificate(sslCert, argv[0], "US", "test"))
        return ret;

    {
        QFile file(QString::fromUtf8(sslCert));
        if(!file.open(QIODevice::ReadOnly))
            return -2;

        nx::network::SslSocket::initSSLEngine(file.readAll());
    }

    const int result = RUN_ALL_TESTS();
    QFile::remove(QString::fromUtf8(sslCert));
    return result;
}

bool readCmdArguments(int argc, char **argv)
{
    std::multimap<QString, QString> args;
    parseCmdArgs(argc, argv, &args);

    const auto enforceSocketTypeIter = args.find("enforce-socket");
    if (enforceSocketTypeIter != args.end())
        SocketFactory::enforceStreamSocketType(enforceSocketTypeIter->second);

    SocketFactory::enforceSsl(args.count("enforce-ssl"));

    //testing whether any logging argument is given...
    const auto anyLogArgIter = args.lower_bound("log");
    if (anyLogArgIter != args.end() && anyLogArgIter->first.startsWith("log"))
    {
        QString logLevel("DEBUG");
        const auto logLevelIter = args.find("log-level");
        if (logLevelIter != args.end())
        {
            logLevel = logLevelIter->second;
            QnLog::initLog(logLevel);
        }

        QString logFile("nx_network_ut.log");
        const auto logFileIter = args.find("log-file");
        if (logFileIter != args.end())
        {
            logFile = logFileIter->second;
            cl_log.create(
                logFile,
                1024 * 1024 * 10,
                5,
                QnLog::logLevelFromString(logLevel));
        }
    }

    return true;
}
