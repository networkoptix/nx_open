// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_startup_parameters.h"

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <shellapi.h>
#endif

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/branding.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/common/utils/encoded_string.h>
#include <nx/vms/client/desktop/ini.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/util.h>

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

static const QString kEncodeAuthMagic_v30 = "@@";
static const QString kEncodeAuthMagic_v50_token = "@T@";
static const nx::utils::SoftwareVersion kTokenSupportVersion(5, 0);

constexpr std::string_view kUserTypeParam = "user_type";

QString fromNativePath(const QString &path)
{
    QString result = QDir::cleanPath(QDir::fromNativeSeparators(path));

    if (!result.isEmpty() && result.endsWith(QLatin1Char('/')))
        result.chop(1);

    return result;
}

} // namespace

const QString QnStartupParameters::kAuthenticationStringKey(lit("--auth"));
const QString QnStartupParameters::kAllowMultipleClientInstancesKey("--no-single-application");
const QString QnStartupParameters::kSelfUpdateKey(lit("--self-update"));

const QString QnStartupParameters::kSessionIdKey(lit("--sessionId"));
const QString QnStartupParameters::kStateFileKey(lit("--stateFile"));

const QString QnStartupParameters::kScreenKey(lit("--screen"));
const QString QnStartupParameters::kFullScreenDisabledKey(lit("--no-fullscreen"));
const QString QnStartupParameters::kWindowGeometryKey(lit("--window-geometry"));
const QString QnStartupParameters::kSkipAutoLoginKey("--skip-auto-login");

QnStartupParameters QnStartupParameters::fromCommandLineArg(int argc, char** argv)
{
    QnStartupParameters result;

    QnCommandLineParser commandLineParser;
    QStringList unparsed;
    commandLineParser.storeUnparsed(&unparsed);

    /* Options used to open new client window. */
    addParserParam(commandLineParser, &result.allowMultipleClientInstances, kAllowMultipleClientInstancesKey);
    addParserParam(commandLineParser, &result.authenticationString, kAuthenticationStringKey );
    addParserParam(commandLineParser, &result.screen, kScreenKey);
    addParserParam(commandLineParser, &result.delayedDrop, "--delayed-drop");
    addParserParam(commandLineParser, &result.instantDrop, "--instant-drop");
    addParserParam(commandLineParser, &result.layoutRef, "--layout-name");

    /* Development options */
    addParserParam(commandLineParser, &result.softwareYuv,          "--soft-yuv");
    addParserParam(commandLineParser, &result.forceLocalSettings,   "--local-settings");
    addParserParam(commandLineParser, &result.fullScreenDisabled,   kFullScreenDisabledKey);
    addParserParam(commandLineParser, &result.skipMediaFolderScan,  "--skip-media-folder-scan");
    addParserParam(commandLineParser, &result.skipAutoLogin, kSkipAutoLoginKey);
    addParserParam(commandLineParser, &result.engineVersion,        "--override-version");
    addParserParam(commandLineParser, &result.vmsProtocolVersion,   "--override-protocol-version");
    addParserParam(commandLineParser, &result.showFullInfo,         "--show-full-info");
    addParserParam(commandLineParser, &result.exportedMode,         "--exported");
    addParserParam(commandLineParser, &result.selfUpdateMode,       kSelfUpdateKey);
    addParserParam(commandLineParser, &result.ipVersion,            "--ip-version");
    addParserParam(commandLineParser, &result.scriptFile,           "--script-file");
    addParserParam(commandLineParser, &result.traceFile,            "--trace-file");

    /* Persistent settings override. */
    addParserParam(commandLineParser, &result.logLevel, "--log-level");
    addParserParam(commandLineParser, &result.logFile, "--log-file");

    addParserParam(commandLineParser, &result.clientUpdateDisabled, "--no-client-update");

    /* Runtime settings */
    addParserParam(commandLineParser, &result.acsMode, "--acs");

    /* Custom uri handling */
    QString strCustomUri;
    addParserParam(commandLineParser, &strCustomUri, QString("%1://").arg(
        nx::branding::nativeUriProtocol()).toUtf8().constData());

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

    QString windowGeometry;
    addParserParam(commandLineParser, &windowGeometry, kWindowGeometryKey);

    addParserParam(commandLineParser, &result.sessionId, kSessionIdKey);
    addParserParam(commandLineParser, &result.stateFileName, kStateFileKey);

    // TODO: #sivanov Reimplement wide-char command-line parsing.
    // This is a temporary solution to avoid massive refactoring before release. More correclty is
    // to process this in the main.cpp (using wmain entry) and pass arguments as QStringList to the
    // module runApplication routine.
    #if defined(Q_OS_WIN)
        int wargc = 0;
        LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
        NX_ASSERT(wargc == argc, "Command-line parsing error");
        if (wargv)
        {
            QStringList arguments;
            for (int i = 0; i < wargc; i++)
                arguments.push_back(QString::fromWCharArray(wargv[i]));

            commandLineParser.parse(arguments, stderr);
            // Free memory allocated for CommandLineToArgvW arguments.
            LocalFree(wargv);
        }
        else
        {
            // Fallback routine.
            commandLineParser.parse(argc, (const char**) argv, stderr);
        }
    #else
        commandLineParser.parse(argc, (const char**) argv, stderr);
    #endif

    if (!strCustomUri.isEmpty())
    {
        /* Restore protocol part that was cut out. */
        QString fixedUri = QString("%1://%2").arg(
            nx::branding::nativeUriProtocol()).arg(strCustomUri);
        NX_DEBUG(NX_SCOPE_TAG, "Run with custom URI %1", fixedUri);
        result.customUri = nx::vms::utils::SystemUri(fixedUri);
        NX_DEBUG(NX_SCOPE_TAG, "Parsed to %1", result.customUri.toUrl());
    }
    result.videoWallGuid = nx::Uuid(strVideoWallGuid);
    result.videoWallItemGuid = nx::Uuid(strVideoWallItemGuid);

    result.qmlImportPaths = qmlImportPaths.split(',', Qt::SkipEmptyParts);

    if (!windowGeometry.isEmpty())
        QnLexical::deserialize(windowGeometry, &result.windowGeometry);

    for (const QString& arg: unparsed)
    {
        // String was created using ::fromUtf8 conversion.
        const QString& fileName = QFile::decodeName(arg.toUtf8());
        result.files.append(fromNativePath(fileName));
    }

    return result;
}

QString QnStartupParameters::createAuthenticationString(
    const nx::vms::client::core::LogonData& logonData,
    std::optional<nx::utils::SoftwareVersion> version)
{
    auto builder = nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(logonData.address)
        .addQueryItem(kUserTypeParam, logonData.userType);

    // Login to cloud is allowed using bearer token only.
    const bool isCloudLogin = (logonData.userType == nx::vms::api::UserType::cloud)
        && logonData.credentials.authToken.isBearerToken();
    if (!isCloudLogin)
    {
        builder.setUserName(QString::fromStdString(logonData.credentials.username));
        builder.setPassword(QString::fromStdString(logonData.credentials.authToken.value));
    }

    const auto logonInformation = QString::fromUtf8(builder.toUrl().toEncoded());
    const QString encoded =
        nx::vms::client::core::EncodedString::fromDecoded(logonInformation).encoded();

    if ((version && *version < kTokenSupportVersion)
        || logonData.credentials.authToken.isPassword())
    {
        return kEncodeAuthMagic_v30 + encoded;
    }

    return kEncodeAuthMagic_v50_token + encoded;
}

nx::vms::client::core::LogonData QnStartupParameters::parseAuthenticationString(QString string)
{
    if (string.isEmpty())
        return {};

    nx::network::http::AuthTokenType authTokenType = nx::network::http::AuthTokenType::password;

    if (string.startsWith(kEncodeAuthMagic_v50_token))
    {
        string = string.mid(kEncodeAuthMagic_v50_token.length());
        string = nx::vms::client::core::EncodedString::fromEncoded(string).decoded();
        authTokenType = nx::network::http::AuthTokenType::bearer;
    }
    else if (string.startsWith(kEncodeAuthMagic_v30))
    {
        string = string.mid(kEncodeAuthMagic_v30.length());
        string = nx::vms::client::core::EncodedString::fromEncoded(string).decoded();
    }

    const nx::utils::Url url = nx::utils::Url::fromUserInput(string);
    const auto query = nx::utils::UrlQuery(url.query());

    nx::vms::client::core::LogonData result;
    result.address = nx::network::SocketAddress::fromUrl(url);
    result.credentials.username = url.userName().toStdString();
    result.credentials.authToken.type = authTokenType;
    result.credentials.authToken.value = url.password().toStdString();
    result.userType = nx::reflect::fromString<nx::vms::api::UserType>(
        query.queryItemValue<std::string>(kUserTypeParam), result.userType);
    return result;
}

nx::vms::client::core::LogonData QnStartupParameters::parseAuthenticationString() const
{
    return parseAuthenticationString(authenticationString);
}

bool QnStartupParameters::isVideoWallMode() const
{
    return !videoWallGuid.isNull();
}
