#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <src/context/context.h>

#include <api/app_server_connection.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    Context context;

    QnAppServerConnectionFactory::setEC2ConnectionFactory(new ec2::)

    QQmlApplicationEngine engine(&context);
    engine.load(QUrl(QStringLiteral("qrc:///src/main.qml")));

    return app.exec();
}
