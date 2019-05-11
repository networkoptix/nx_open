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
        "qml-root",
        "The directory which contains runtime ui resources: 'qml' and 'images'.",
        "qmlRoot");
    parser.addOption(qmlRootOption);

    const auto liteModeOption = QCommandLineOption(
        "lite-mode",
        "Enable lite mode.");
    parser.addOption(liteModeOption);

    const auto logLevelOption = QCommandLineOption(
        "log-level",
        "Log level.",
        "logLevel");
    parser.addOption(logLevelOption);

    const auto urlOption = QCommandLineOption(
        "url",
        "URL to be used for server connection instead of asking login/password.",
        "url");
    parser.addOption(urlOption);

    auto videowallInstanceGuidOption = QCommandLineOption(
        "videowall-instance-guid",
        "GUID which is used to check Videowall Control messages.",
        "videowallInstanceGuid");
    parser.addOption(videowallInstanceGuidOption);
    videowallInstanceGuidOption.setHidden(true);

    const auto autoLoginModeOption = QCommandLineOption(
        "auto-login",
        "Auto-login mode: enabled, disabled or auto (default).",
        "autoLoginMode");
    parser.addOption(autoLoginModeOption);

    auto testOption = QCommandLineOption(
        "test",
        "Enable test.",
        "test");
    testOption.setHidden(true);
    parser.addOption(testOption);

    auto webSocketPortOption = QCommandLineOption(
       "webSocketPort",
       "WEB socket port.",
       "webSocketPort");
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
        if (value == "disabled")
            autoLoginMode = AutoLoginMode::Disabled;
        else if (value == "enabled")
            autoLoginMode = AutoLoginMode::Enabled;
        else if (value == "auto")
            autoLoginMode = AutoLoginMode::Auto;
    }

    if (parser.isSet(testOption))
    {
        testMode = true;
        initialTest = parser.value(testOption);
    }

    if (parser.isSet(webSocketPortOption))
        webSocketPort = parser.value(webSocketPortOption).toShort();
}
