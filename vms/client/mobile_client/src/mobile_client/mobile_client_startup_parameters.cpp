#include "mobile_client_startup_parameters.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>

QnMobileClientStartupParameters::QnMobileClientStartupParameters()
{}

QnMobileClientStartupParameters::QnMobileClientStartupParameters(
    const QCoreApplication& application)
{
    QCommandLineParser parser;

    const auto qmlRootOption = QCommandLineOption(
        lit("qml-root"),
        lit("The directory which contains runtime ui resources: 'qml' and 'images'."),
        lit("qmlRoot"));
    parser.addOption(qmlRootOption);

    const auto liteModeOption = QCommandLineOption(
        lit("lite-mode"),
        lit("Enable lite mode."));
    parser.addOption(liteModeOption);

    const auto logLevelOption = QCommandLineOption(
        lit("log-level"),
        lit("Log level."),
        lit("logLevel"));
    parser.addOption(logLevelOption);

    const auto urlOption = QCommandLineOption(
        lit("url"),
        lit("URL to be used for server connection instead of asking login/password."),
        lit("url"));
    parser.addOption(urlOption);

    auto videowallInstanceGuidOption = QCommandLineOption(
        lit("videowall-instance-guid"),
        lit("GUID which is used to check Videowall Control messages."),
        lit("videowallInstanceGuid"));
    parser.addOption(videowallInstanceGuidOption);
    videowallInstanceGuidOption.setHidden(true);

    const auto autoLoginModeOption = QCommandLineOption(
        lit("auto-login"),
        lit("Auto-login mode: enabled, disabled or auto (default)."),
        lit("autoLoginMode"));
    parser.addOption(autoLoginModeOption);

    auto testOption = QCommandLineOption(
        lit("test"),
        lit("Enable test."),
        lit("test"));
    testOption.setHidden(true);
    parser.addOption(testOption);

    auto webSocketPortOption = QCommandLineOption(
       lit("webSocketPort"),
       lit("WEB socket port."),
       lit("webSocketPort"));
    webSocketPortOption.setHidden(true);
    parser.addOption(webSocketPortOption);

    parser.parse(application.arguments());

    if (parser.isSet(qmlRootOption))
        qmlRoot = parser.value(qmlRootOption);

    liteMode = parser.isSet(liteModeOption);

    if (parser.isSet(logLevelOption))
        logLevel = parser.value(logLevelOption);

    if (parser.isSet(urlOption))
        url = parser.value(urlOption);

    if (parser.isSet(videowallInstanceGuidOption))
        videowallInstanceGuid = QnUuid::fromStringSafe(parser.value(videowallInstanceGuidOption));

    if (parser.isSet(autoLoginModeOption))
    {
        const auto value = parser.value(autoLoginModeOption);
        if (value == lit("disabled"))
            autoLoginMode = AutoLoginMode::Disabled;
        else if (value == lit("enabled"))
            autoLoginMode = AutoLoginMode::Enabled;
        else if (value == lit("auto"))
            autoLoginMode = AutoLoginMode::Auto;
    }

    if (parser.isSet(testOption))
    {
        testMode = true;
        initialTest = parser.value(testOption);
    }

    if (parser.isSet(webSocketPortOption))
        webSocketPort = parser.value(webSocketPortOption).toUShort();
}
