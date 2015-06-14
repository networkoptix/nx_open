#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <QtQml>
#include <models/ip_settings_model.h>
#include <models/servers_selection_model.h>
#include <base/constants.h>
#include <base/rtu_context.h>

void registerTypes()
{
    
    const char kRtuDomainName[] = "networkoptix.rtu";

    enum 
    {
        kMinorVersion = 0
        , kMajorVersion = 1 
    };
    
    const char kServerFlagsQmlTypeName[] = "Constants";

    qmlRegisterUncreatableType<rtu::Constants>(kRtuDomainName
        , kMajorVersion, kMinorVersion, kServerFlagsQmlTypeName, kServerFlagsQmlTypeName);
}

#include <helpers/time_helper.h>
#include <QFont>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    rtu::RtuContext rtuContext(&app);
    
    QQmlApplicationEngine engine;
        
    registerTypes();
    
    engine.rootContext()->setContextProperty("rtuContext", &rtuContext);
    engine.load(QUrl(QStringLiteral("qrc:/src/qml/main.qml")));

    return app.exec();
}
