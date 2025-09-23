// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_startup_parameters.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace {

QRect fromString(const QString& str)
{
    const QJsonObject& obj = QJsonDocument::fromJson(str.toLatin1()).object();
    if (obj.isEmpty())
        return {};

    return QRect{
        obj.value("x").toInt(),
        obj.value("y").toInt(),
        obj.value("width").toInt(),
        obj.value("height").toInt()};
}

} // namespace

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

    const auto qmlImportPathsOption = QCommandLineOption(
        "qml-import-paths",
        "Comma-separated list of QML import paths.",
        "qmlImportPaths");
    parser.addOption(qmlImportPathsOption);

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
    videowallInstanceGuidOption.setFlags(QCommandLineOption::HiddenFromHelp);

    auto testOption = QCommandLineOption(
        "test",
        "Enable test.",
        "test");
    testOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(testOption);

    auto windowGeometryOption = QCommandLineOption(
        "window-geometry",
        "Set window geometry.",
        "windowGeometry");
    windowGeometryOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(windowGeometryOption);

    parser.parse(application.arguments());

    if (parser.isSet(qmlRootOption))
        qmlRoot = parser.value(qmlRootOption);

    if (parser.isSet(qmlImportPathsOption))
        qmlImportPaths = parser.value(qmlImportPathsOption).split(',', Qt::SkipEmptyParts);

    if (parser.isSet(logLevelOption))
        logLevel = parser.value(logLevelOption);

    if (parser.isSet(urlOption))
        url = parser.value(urlOption);

    if (parser.isSet(windowGeometryOption))
        windowGeometry = fromString(parser.value(windowGeometryOption));
}
