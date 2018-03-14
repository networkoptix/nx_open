#include <QtCore/QString>
#include <QtCore/QScopedPointer>
#include <QtCore/QDebug>
#include <QtCore/QRect>

#include <QtGui/QWindow>
#include <QtGui/QScreen>

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QTextEdit>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

static QStringList* globalLog = nullptr;

static void debugMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (!globalLog)
        return;

    globalLog->append(msg);
}

int runApplication(QApplication* application)
{
    QScopedPointer<QWidget> mainWindow(new QWidget());
    mainWindow->setAttribute(Qt::WA_QuitOnClose);

    auto layout = new QVBoxLayout(mainWindow.data());

    auto logWindow = new QTextEdit(mainWindow.data());
    layout->addWidget(logWindow);
    logWindow->setReadOnly(true);

    QStringList log;
    globalLog = &log;

    const auto previousMsgHandler = qInstallMessageHandler(debugMsgHandler);

    QDesktopWidget *desktop = qApp->desktop();
    qDebug() << "System screen configuration:";
    for (int i = 0; i < desktop->screenCount(); ++i)
    {
        const auto screen = QGuiApplication::screens().value(i);
        qDebug() << "Screen " << i << "geometry is" << screen->geometry();
        if (desktop->screenGeometry(i) != screen->geometry())
            qDebug() << "Alternative screen" << i << "geometry is" << desktop->screenGeometry(i);
    }
    mainWindow->show();

    logWindow->setPlainText(log.join('\n'));
    const auto result = application->exec();

    qInstallMessageHandler(previousMsgHandler);
    globalLog = nullptr;

    return result;
}


int main(int argc, char** argv)
{
    // These attributes must be set before application instance is created.
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QScopedPointer<QApplication> application(new QApplication(argc, argv));

    //adding exe dir to plugin search path
    QStringList pluginDirs = QCoreApplication::libraryPaths();
    pluginDirs << QCoreApplication::applicationDirPath();
    QCoreApplication::setLibraryPaths(pluginDirs);

    return runApplication(application.data());
}
