#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <QtQml>
#include <models/ip_settings_model.h>
#include <models/servers_selection_model.h>
#include <constants.h>
#include <rtu_context.h>

void registerTypes()
{
    
    const char kRtuDomainName[] = "networkoptix.rtu";

    enum 
    {
        kMinorVersion = 0
        , kMajorVersion = 1 
    };
    
    const char kServerFlagsQmlTypeName[] = "Constants";
    qmlRegisterType<rtu::Constants>(kRtuDomainName, 1, 0, kServerFlagsQmlTypeName);
    
/*
    const char kIpSettingsModelTypeName[] = "IpSettingsModel";
    qmlRegisterType<rtu::IpSettingsModel>(kRtuDomainName, 1, 0, kIpSettingsModelTypeName);
    */
    /*
    const char kServersSelectionListModelTypeName[] = "ServersSelectionListModel";
    qmlRegisterType<rtu::ServersSelectionModel>(kRtuDomainName, rtu::ServersSelectionModel::kVersionMajor
        , rtu::ServersSelectionModel::kVersionMinor, kServersSelectionListModelTypeName);
    */
    /*
    const char kRtuContextTypeName[] = "ServersSelectionListModel";
    qmlRegisterType<rtu::ServersSelectionModel>(kRtuDomainName, rtu::ServersSelectionModel::kVersionMajor
        , rtu::ServersSelectionModel::kVersionMinor, kServersSelectionListModelTypeName);
        */
    
    
}

int main(int argc, char *argv[])
{
            
    QGuiApplication app(argc, argv);

    rtu::RtuContext rtuContext(&app);
    
    QQmlApplicationEngine engine;
    
    registerTypes();

    engine.rootContext()->setContextProperty("rtuContext", &rtuContext);
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    return app.exec();
}
