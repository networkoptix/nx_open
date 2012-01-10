#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QMainWindow>

class QTabWidget;

class QnBlueBackgroundPainter;
class QnGraphicsView;
class QnWorkbench;
class QnWorkbenchController;
class QnWorkbenchDisplay;

class MainWnd : public QMainWindow
{
    Q_OBJECT

public:
    MainWnd(int argc, char* argv[], QWidget *parent = 0, Qt::WFlags flags = 0);
    virtual ~MainWnd();

    static MainWnd* instance() { return s_instance; }

Q_SIGNALS:
    void mainWindowClosed();

protected:
    void closeEvent(QCloseEvent *event);

private Q_SLOTS:
    void addTab();
    void currentTabChanged(int index);
    void closeTab(int index);

    void handleMessage(const QString &message);

    void openFile();

    void activate();
    void toggleFullScreen();
    void editPreferences();

    void appServerError(int error);
    void appServerAuthenticationRequired();

    void toggleDecorationsVisibility();

#if 0
public:
    void addFilesToCurrentOrNewLayout(const QStringList& files, bool forceNewLayout = false);
    void goToNewLayoutContent(LayoutContent* newl);

private:
    void destroyNavigator(CLLayoutNavigator *&nav);

    CLLayoutNavigator *m_normalView;
#endif

private:
    QScopedPointer<QnBlueBackgroundPainter> m_backgroundPainter;
    QnWorkbenchController *m_controller;
    QnWorkbenchDisplay *m_display;
    QnWorkbench *m_workbench;
    QnGraphicsView *m_view;

    QTabWidget *m_tabWidget;
    QToolBar *m_toolBar;

    static MainWnd *s_instance;
};

#endif // MAINWND_H
