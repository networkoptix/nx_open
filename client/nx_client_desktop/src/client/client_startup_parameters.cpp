#include "client_startup_parameters.h"

#include <QtCore/QFile>

#include <utils/common/app_info.h>
#include <utils/common/command_line_parser.h>
#include <nx/utils/cryptographic_hash.h>
#include <utils/common/util.h>
#include <utils/crypt/encoded_string.h>

#include <nx/vms/utils/app_info.h>
#include <nx/utils/log/log.h>

namespace
{
    template<typename ValueType>
    void addParserParam(QnCommandLineParser &parser
        , ValueType *valuePtr
        , const char *longName)
    {
        parser.addParameter(valuePtr, longName, nullptr, QString());
    };

    template<typename ValueType>
    void addParserParam(QnCommandLineParser &parser
                        , ValueType *valuePtr
                        , const QString& longName)
    {
        parser.addParameter(valuePtr, longName, QString(), QString());
    };

    template<typename ValueType, typename DefaultParamType>
    void addParserParam(QnCommandLineParser &parser
        , ValueType *valuePtr
        , const char *longName
        , const DefaultParamType &defaultParam)
    {
        parser.addParameter(valuePtr, longName, nullptr, QString(), defaultParam);
    };

    static const QString kEncodeAuthMagic = lit("@@");
    static const QnSoftwareVersion kEncodeSupportVersion(3, 0, 0, 14350);

}

const QString QnStartupParameters::kScreenKey(lit("--screen"));
const QString QnStartupParameters::kAllowMultipleClientInstancesKey(lit("--no-single-application"));
const QString QnStartupParameters::kSelfUpdateKey(lit("--self-update"));

QnStartupParameters QnStartupParameters::fromCommandLineArg(int argc, char** argv)
{
    QnStartupParameters result;

    QnCommandLineParser commandLineParser;
    QStringList unparsed;
    commandLineParser.storeUnparsed(&unparsed);

    /* Options used to open new client window. */
    addParserParam(commandLineParser, &result.allowMultipleClientInstances, kAllowMultipleClientInstancesKey);
    addParserParam(commandLineParser, &result.authenticationString, "--auth" );
    addParserParam(commandLineParser, &result.screen, kScreenKey);
    addParserParam(commandLineParser, &result.delayedDrop, "--delayed-drop");
    addParserParam(commandLineParser, &result.instantDrop, "--instant-drop");

    /* Development options */
    addParserParam(commandLineParser, &result.dynamicCustomizationPath,"--customization");
    addParserParam(commandLineParser, &result.devModeKey,           "--dev-mode-key");
    addParserParam(commandLineParser, &result.softwareYuv,          "--soft-yuv");
    addParserParam(commandLineParser, &result.forceLocalSettings,   "--local-settings");
    addParserParam(commandLineParser, &result.fullScreenDisabled,   "--no-fullscreen");
    addParserParam(commandLineParser, &result.skipMediaFolderScan,  "--skip-media-folder-scan");
    addParserParam(commandLineParser, &result.engineVersion,        "--override-version");
    addParserParam(commandLineParser, &result.showFullInfo,         "--show-full-info");
    addParserParam(commandLineParser, &result.exportedMode,         "--exported");
    addParserParam(commandLineParser, &result.hiDpiDisabled,        "--no-hidpi");
    addParserParam(commandLineParser, &result.selfUpdateMode,       kSelfUpdateKey);
    addParserParam(commandLineParser, &result.ipVersion,            "--ip-version");

    /* Persistent settings override. */
    addParserParam(commandLineParser, &result.logLevel, "--log-level");
    addParserParam(commandLineParser, &result.ec2TranLogLevel, "--ec2-tran-log-level", lit("none"));

    addParserParam(commandLineParser, &result.clientUpdateDisabled, "--no-client-update");
    addParserParam(commandLineParser, &result.vsyncDisabled, "--no-vsync");
    addParserParam(commandLineParser, &result.profilerMode, "--profiler");

    /* Runtime settings */
    addParserParam(commandLineParser, &result.ignoreVersionMismatch, "--no-version-mismatch-check");

    /* Custom uri handling */
    QString strCustomUri;
    addParserParam(commandLineParser, &strCustomUri, lit("%1://").arg(nx::vms::utils::AppInfo::nativeUriProtocol()).toUtf8().constData());

    QString strVideoWallGuid;
    QString strVideoWallItemGuid;
    addParserParam(commandLineParser, &strVideoWallGuid, "--videowall");
    addParserParam(commandLineParser, &strVideoWallItemGuid, "--videowall-instance");
    addParserParam(commandLineParser, &result.lightMode, "--light-mode", lit("full"));
    addParserParam(commandLineParser, &result.enforceSocketType, "--enforce-socket");
    addParserParam(commandLineParser, &result.enforceMediatorEndpoint, "--enforce-mediator");

    commandLineParser.parse(argc, (const char**) argv, stderr);

    if (!strCustomUri.isEmpty())
    {
        /* Restore protocol part that was cut out. */
        QString fixedUri = lit("%1://%2").arg(nx::vms::utils::AppInfo::nativeUriProtocol()).arg(strCustomUri);
        NX_LOG(lit("Run with custom URI %1").arg(fixedUri), cl_logDEBUG1);
        result.customUri = nx::vms::utils::SystemUri(fixedUri);
        NX_LOG(lit("Parsed to %1").arg(result.customUri.toString()), cl_logDEBUG1);
    }
    result.videoWallGuid = QnUuid(strVideoWallGuid);
    result.videoWallItemGuid = QnUuid(strVideoWallItemGuid);

    // First unparsed entry is the application path.
    NX_EXPECT(!unparsed.empty());
    for (int i = 1; i < unparsed.size(); ++i)
    {
        const auto source = unparsed[i].toUtf8(); //< String was created using ::fromUtf8 conversion
        QString fileName = QFile::decodeName(source);
        result.files.append(fromNativePath(fileName));
    }

    return result;
}

QString QnStartupParameters::createAuthenticationString(const QUrl& url,
    const QnSoftwareVersion& version)
{
    // For old clients use compatible format
    if (!version.isNull() && version < kEncodeSupportVersion)
        return QString::fromUtf8(url.toEncoded());

    QnEncodedString encoded(QString::fromUtf8(url.toEncoded()));
    return kEncodeAuthMagic + encoded.encoded();
}

QUrl QnStartupParameters::parseAuthenticationString(QString string)
{
    if (string.startsWith(kEncodeAuthMagic))
    {
        string = string.mid(kEncodeAuthMagic.length());
        string = QnEncodedString::fromEncoded(string).value();
    }

    return QUrl::fromUserInput(string);
}

QUrl QnStartupParameters::parseAuthenticationString() const
{
    return parseAuthenticationString(authenticationString);
}

bool QnStartupParameters::isDevMode() const
{
    /* MD5("razrazraz") */
    return nx::utils::QnCryptographicHash::hash(devModeKey.toLatin1(), nx::utils::QnCryptographicHash::Md5)
        == QByteArray("\x4f\xce\xdd\x9b\x93\x71\x56\x06\x75\x4b\x08\xac\xca\x2d\xbc\x7f");
}
