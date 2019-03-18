#include "client_startup_parameters.h"

#include <QtCore/QFile>

#include <nx/vms/client/core/common/utils/encoded_string.h>

#include <utils/common/app_info.h>
#include <utils/common/command_line_parser.h>
#include <nx/utils/cryptographic_hash.h>
#include <utils/common/util.h>

#include <nx/vms/utils/app_info.h>
#include <nx/utils/log/log.h>

#include <nx/vms/client/desktop/ini.h>

namespace {

template<typename ValueType>
void addParserParam(QnCommandLineParser& parser, ValueType* valuePtr, const char* longName)
{
    parser.addParameter(valuePtr, longName, nullptr, QString());
}

template<typename ValueType>
void addParserParam(QnCommandLineParser& parser, ValueType* valuePtr, const QString& longName)
{
    parser.addParameter(valuePtr, longName, QString(), QString());
}

template<typename ValueType, typename DefaultParamType>
void addParserParam(QnCommandLineParser& parser, ValueType* valuePtr, const char* longName,
    const DefaultParamType& defaultParam)
{
    parser.addParameter(valuePtr, longName, nullptr, QString(), defaultParam);
};

static const QString kEncodeAuthMagic = lit("@@");
static const nx::vms::api::SoftwareVersion kEncodeSupportVersion(3, 0, 0, 14350);

QString fromNativePath(const QString &path)
{
    QString result = QDir::cleanPath(QDir::fromNativeSeparators(path));

    if (!result.isEmpty() && result.endsWith(QLatin1Char('/')))
        result.chop(1);

    return result;
}

} // namespace

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
    addParserParam(commandLineParser, &result.layoutRef, "--layout-name");

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
    addParserParam(commandLineParser, &result.logFile, "--log-file");

    addParserParam(commandLineParser, &result.clientUpdateDisabled, "--no-client-update");
    addParserParam(commandLineParser, &result.vsyncDisabled, "--no-vsync");
    addParserParam(commandLineParser, &result.profilerMode, "--profiler");

    /* Runtime settings */
    addParserParam(commandLineParser, &result.acsMode, "--acs");

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

    addParserParam(commandLineParser, &result.qmlRoot, "--qml-root");
    QString qmlImportPaths;
    addParserParam(commandLineParser, &qmlImportPaths, "--qml-import-paths");

    commandLineParser.parse(argc, (const char**) argv, stderr);

    if (!strCustomUri.isEmpty())
    {
        /* Restore protocol part that was cut out. */
        QString fixedUri = lit("%1://%2").arg(nx::vms::utils::AppInfo::nativeUriProtocol()).arg(strCustomUri);
        NX_DEBUG(typeid(QnStartupParameters), "Run with custom URI %1", fixedUri);
        result.customUri = nx::vms::utils::SystemUri(fixedUri);
        NX_DEBUG(typeid(QnStartupParameters), "Parsed to %1", result.customUri.toUrl());
    }
    result.videoWallGuid = QnUuid(strVideoWallGuid);
    result.videoWallItemGuid = QnUuid(strVideoWallItemGuid);

    result.qmlImportPaths = qmlImportPaths.split(',', QString::SkipEmptyParts);

    // First unparsed entry is the application path.
    NX_ASSERT(!unparsed.empty());
    for (int i = 1; i < unparsed.size(); ++i)
    {
        const auto source = unparsed[i].toUtf8(); //< String was created using ::fromUtf8 conversion
        QString fileName = QFile::decodeName(source);
        result.files.append(fromNativePath(fileName));
    }

    return result;
}

QString QnStartupParameters::createAuthenticationString(const nx::utils::Url& url,
    const nx::vms::api::SoftwareVersion& version)
{
    const auto logonInformation = QString::fromUtf8(url.toEncoded());

    // For old clients use compatible format
    if (!version.isNull() && version < kEncodeSupportVersion)
        return logonInformation;

    // TODO: #GDM Possibly it's better to add extra key to the logon info as a salt.
    const auto secure = nx::vms::client::core::EncodedString::fromDecoded(logonInformation);
    return kEncodeAuthMagic + secure.encoded();
}

nx::utils::Url QnStartupParameters::parseAuthenticationString(QString string)
{
    if (string.startsWith(kEncodeAuthMagic))
    {
        string = string.mid(kEncodeAuthMagic.length());
        string = nx::vms::client::core::EncodedString::fromEncoded(string).decoded();
    }

    return nx::utils::Url::fromUserInput(string);
}

nx::utils::Url QnStartupParameters::parseAuthenticationString() const
{
    return parseAuthenticationString(authenticationString);
}

bool QnStartupParameters::isDevMode() const
{
    // MD5("razrazraz").
    const auto developerKey =
        QByteArray("\x4f\xce\xdd\x9b\x93\x71\x56\x06\x75\x4b\x08\xac\xca\x2d\xbc\x7f");

    using Hash = nx::utils::QnCryptographicHash;
    return nx::vms::client::desktop::ini().developerMode
        || Hash::hash(devModeKey.toLatin1(), Hash::Md5) == developerKey;
}

bool QnStartupParameters::isVideoWallMode() const
{
    return !videoWallGuid.isNull();
}
