
#include "client_startup_parameters.h"

#include <utils/common/command_line_parser.h>

namespace
{
    const bool kDefaultNoFullScreen =
#ifdef Q_OS_MAC
        true;
#else
        false;
#endif

    template<typename ValueType>
    void addParserParam(QnCommandLineParser &parser
        , ValueType *valuePtr
        , const char *longName)
    {
        parser.addParameter(valuePtr, longName, nullptr, QString());
    };   

    template<typename ValueType, typename DefaultParamType>
    void addParserParam(QnCommandLineParser &parser
        , ValueType *valuePtr
        , const char *longName
        , const DefaultParamType &defaultParam)
    {
        parser.addParameter(valuePtr, longName, nullptr, QString(), defaultParam);
    };    
}

QnStartupParameters QnStartupParameters::fromCommandLineArg(int argc
    , char **argv)
{
    QnStartupParameters result;

    QnCommandLineParser commandLineParser;

    /* Options used to open new client window. */
    addParserParam(commandLineParser, &result.allowMultipleClientInstances, "--no-single-application");
    addParserParam(commandLineParser, &result.authenticationString, "--auth" );
    addParserParam(commandLineParser, &result.screen, "--screen");
    addParserParam(commandLineParser, &result.delayedDrop, "--delayed-drop");
    addParserParam(commandLineParser, &result.instantDrop, "--instant-drop");
    
    /* Development options */
#ifdef ENABLE_DYNAMIC_TRANSLATION
    addParserParam(commandLineParser, &result.dynamicTranslationPath, "--translation");
#endif

#ifdef ENABLE_DYNAMIC_CUSTOMIZATION
    addParserParam(commandLineParser, &result.dynamicCustomizationPath,"--customization");
#endif

    addParserParam(commandLineParser, &result.devModeKey, "--dev-mode-key");
    addParserParam(commandLineParser, &result.softwareYuv, "--soft-yuv");
    addParserParam(commandLineParser, &result.forceLocalSettings, "--local-settings");
    addParserParam(commandLineParser, &result.fullScreenDisabled, "--no-fullscreen");
    addParserParam(commandLineParser, &result.skipMediaFolderScan, "--skip-media-folder-scan");
    addParserParam(commandLineParser, &result.engineVersion, "--override-version");

    /* Persistent settings override. */
    addParserParam(commandLineParser, &result.logLevel, "--log-level");
    addParserParam(commandLineParser, &result.ec2TranLogLevel, "--ec2-tran-log-level", lit("none"));

    addParserParam(commandLineParser, &result.clientUpdateDisabled, "--no-client-update");
    addParserParam(commandLineParser, &result.vsyncDisabled, "--no-vsync");

    /* Runtime settings */
    addParserParam(commandLineParser, &result.versionMismatchCheckDisabled, "--no-version-mismatch-check");

    QString strVideoWallGuid;
    QString strVideoWallItemGuid;
    addParserParam(commandLineParser, &strVideoWallGuid, "--videowall");
    addParserParam(commandLineParser, &strVideoWallItemGuid, "--videowall-instance");
    addParserParam(commandLineParser, &result.lightMode, "--light-mode", lit("full"));

    commandLineParser.parse(argc, argv, stderr, QnCommandLineParser::RemoveParsedParameters);

    result.videoWallGuid = QnUuid(strVideoWallGuid);
    result.videoWallItemGuid = QnUuid(strVideoWallItemGuid);
    return result;
}

QnStartupParameters::QnStartupParameters()
    : screen(kInvalidScreen)

    , allowMultipleClientInstances(false)
    , skipMediaFolderScan(false)
    , versionMismatchCheckDisabled(false)
    , vsyncDisabled(false)
    , clientUpdateDisabled(false)
    , softwareYuv(false)
    , forceLocalSettings(false)
    , fullScreenDisabled(kDefaultNoFullScreen)

    , devModeKey()
    , authenticationString()
    , delayedDrop()
    , instantDrop()
    , logLevel()
    , ec2TranLogLevel()
    , dynamicTranslationPath()
    , lightMode()
    , videoWallGuid()
    , videoWallItemGuid()
    , engineVersion()
    , dynamicCustomizationPath()
{
}
