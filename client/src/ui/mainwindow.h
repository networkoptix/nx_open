#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "fancymainwindow.h"

class QTabBar;

class QnBlueBackgroundPainter;
class QnGraphicsView;
class QnWorkbench;
class QnWorkbenchController;
class QnWorkbenchDisplay;

class MainWindow : public FancyMainWindow
{
    Q_OBJECT

public:
    MainWindow(int argc, char *argv[], QWidget *parent = 0, Qt::WFlags flags = 0);
    virtual ~MainWindow();

Q_SIGNALS:
    void mainWindowClosed();

protected:
    bool eventFilter(QObject *watched, QEvent *event);
    void closeEvent(QCloseEvent *event);

private Q_SLOTS:
    void addTab();
    void currentTabChanged(int index);
    void closeTab(int index);

    void handleMessage(const QString &message);

    void treeWidgetItemActivated(uint resourceId);

    void currentWidgetChanged();

    void openFile();

    void activate();
    void toggleFullScreen();
    void editPreferences();

    void appServerError(int error);
    void appServerAuthenticationRequired();

private:
    QScopedPointer<QnBlueBackgroundPainter> m_backgroundPainter;
    QnWorkbenchController *m_controller;
    QnWorkbenchDisplay *m_display;
    QnWorkbench *m_workbench;
    QnGraphicsView *m_view;

    QTabBar *m_tabBar;
};

#endif // MAINWINDOW_H
