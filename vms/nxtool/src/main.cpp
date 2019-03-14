#include <QtGui/QIcon>
#include <QtGui/QGuiApplication>

#include <QtQml>
#include <QQmlApplicationEngine>

#include <models/buttons.h>
#include <base/constants.h>
#include <base/rtu_context.h>
#include <nx/vms/server/api/server_info.h>

void registerTypes()
{
    const char kRtuDomainName[] = "networkoptix.rtu";

    enum
    {
        kMinorVersion = 0
        , kMajorVersion = 1
    };

    const char kConstantsQmlTypeName[] = "Constants";
    qmlRegisterUncreatableType<rtu::Constants>(kRtuDomainName
        , kMajorVersion, kMinorVersion, kConstantsQmlTypeName, kConstantsQmlTypeName);

    const char kRestApiConstantsQmlTypeName[] = "RestApiConstants";
    qmlRegisterUncreatableType<nx::vms::server::api::Constants>(kRtuDomainName
        , kMajorVersion, kMinorVersion, kRestApiConstantsQmlTypeName, kRestApiConstantsQmlTypeName);

    const char kButtonsModelQmlTypeName[] = "Buttons";
    qmlRegisterType<rtu::Buttons>(kRtuDomainName
        , kMajorVersion, kMinorVersion, kButtonsModelQmlTypeName);
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/nxtool.ico"));

    rtu::RtuContext rtuContext(&app);

    QQmlApplicationEngine engine;

    registerTypes();

    engine.rootContext()->setContextProperty("rtuContext", &rtuContext);
    engine.load(QUrl(QStringLiteral("qrc:/src/qml/main.qml")));

    return app.exec();
}
