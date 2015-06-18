
#include <QtQml>
#include <QGuiApplication>
#include <QQmlApplicationEngine>


#include <models/buttons.h>
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

    const char kButtonsModelQmlTypeName[] = "Buttons";
    qmlRegisterType<rtu::Buttons>(kRtuDomainName
        , kMajorVersion, kMinorVersion, kButtonsModelQmlTypeName);
}

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
