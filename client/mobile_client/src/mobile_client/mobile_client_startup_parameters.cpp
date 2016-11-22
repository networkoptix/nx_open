#include "mobile_client_startup_parameters.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>

QnMobileClientStartupParameters::QnMobileClientStartupParameters()
{}

QnMobileClientStartupParameters::QnMobileClientStartupParameters(
    const QCoreApplication& application)
{
    QCommandLineParser parser;

    const auto basePathOption = QCommandLineOption(
        lit("base-path"),
        lit("The directory which contains runtime ui resources: 'qml' and 'images'."),
        lit("basePath"));
    parser.addOption(basePathOption);

    const auto liteModeOption = QCommandLineOption(
        lit("lite-mode"),
        lit("Enable lite mode."));
    parser.addOption(liteModeOption);

    const auto urlOption = QCommandLineOption(
        lit("url"),
        lit("URL to be used for server connection instead of asking login/password."),
        lit("url"));
    parser.addOption(urlOption);

    const auto videowallInstanceGuidOption = QCommandLineOption(
        lit("videowall-instance-guid"),
        lit("GUID which is used to check Videowall Control messages."),
        lit("videowallInstanceGuid"));
    parser.addOption(videowallInstanceGuidOption);

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

    if (parser.isSet(basePathOption))
        basePath = parser.value(basePathOption);

    liteMode = parser.isSet(liteModeOption);

    if (parser.isSet(urlOption))
        url = parser.value(urlOption);

    if (parser.isSet(videowallInstanceGuidOption))
        videowallInstanceGuid = QnUuid::fromStringSafe(parser.value(videowallInstanceGuidOption));

    if (parser.isSet(testOption))
    {
        testMode = true;
        initialTest = parser.value(testOption);
    }

    if (parser.isSet(webSocketPortOption))
        webSocketPort = parser.value(webSocketPortOption).toUShort();
}
