// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>

#include <nx/utils/app_info.h>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(
        "applicationFullVersion", nx::utils::AppInfo::applicationFullVersion());

    engine.load("qrc:///qml/Main.qml");

    return app.exec();
}
