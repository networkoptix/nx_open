
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
    addParserParam(commandLineParser, &result.noSingleApplication, "--no-single-application");
    addParserParam(commandLineParser, &result.authenticationString, "--auth" );
    addParserParam(commandLineParser, &result.screen, "--screen");
    addParserParam(commandLineParser, &result.delayedDrop, "--delayed-drop");
    addParserParam(commandLineParser, &result.instantDrop, "--instant-drop");
    
    /* Development options */
#ifdef ENABLE_DYNAMIC_TRANSLATION
    addParserParam(commandLineParser, &result.translationPath, "--translation");
#endif

#ifdef ENABLE_DYNAMIC_CUSTOMIZATION
    addParserParam(commandLineParser, &result.dynamicCustomizationPath,"--customization");
#endif

    addParserParam(commandLineParser, &result.devModeKey, "--dev-mode-key");
    addParserParam(commandLineParser, &result.softwareYuv, "--soft-yuv");
    addParserParam(commandLineParser, &result.forceLocalSettings, "--local-settings");
    addParserParam(commandLineParser, &result.noFullScreen, "--no-fullscreen");
    addParserParam(commandLineParser, &result.skipMediaFolderScan, "--skip-media-folder-scan");
    addParserParam(commandLineParser, &result.engineVersion, "--override-version");

    /* Persistent settings override. */
    addParserParam(commandLineParser, &result.logLevel, "--log-level");
    addParserParam(commandLineParser, &result.ec2TranLogLevel, "--ec2-tran-log-level", lit("none"));

    addParserParam(commandLineParser, &result.noClientUpdate, "--no-client-update");
    addParserParam(commandLineParser, &result.noVSync, "--no-vsync");

    /* Runtime settings */
    addParserParam(commandLineParser, &result.noVersionMismatchCheck, "--no-version-mismatch-check");
    addParserParam(commandLineParser, &result.sVideoWallGuid, "--videowall");
    addParserParam(commandLineParser, &result.sVideoWallItemGuid,"--videowall-instance");
    addParserParam(commandLineParser, &result.lightMode, "--light-mode", lit("full"));

    commandLineParser.parse(argc, argv, stderr, QnCommandLineParser::RemoveParsedParameters);

    return result;
}

QnStartupParameters QnStartupParameters::createDefault()
{
    return QnStartupParameters();
}

QnStartupParameters::QnStartupParameters()
    : screen(kInvalidScreen)

    , noSingleApplication(false)
    , skipMediaFolderScan(false)
    , noVersionMismatchCheck(false)
    , noVSync(false)
    , noClientUpdate(false)
    , softwareYuv(false)
    , forceLocalSettings(false)
    , noFullScreen(kDefaultNoFullScreen)

    , devModeKey()
    , authenticationString()
    , delayedDrop()
    , instantDrop()
    , logLevel()
    , ec2TranLogLevel()
    , translationPath()
    , lightMode()
    , sVideoWallGuid()
    , sVideoWallItemGuid()
    , engineVersion()
    , dynamicCustomizationPath()
{
}
