#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QMainWindow>

class QSplitter;

class TabWidget;

class CLLayoutNavigator;
class LayoutContent;

class QnBlueBackgroundPainter;
class QnWorkbenchController;

class MainWnd : public QMainWindow
{
    Q_OBJECT

public:
    MainWnd(int argc, char* argv[], QWidget *parent = 0, Qt::WFlags flags = 0);
    virtual ~MainWnd();

    static MainWnd* instance() { return s_instance; }
    void addFilesToCurrentOrNewLayout(const QStringList& files, bool forceNewLayout = false);

    void goToNewLayoutContent(LayoutContent* newl);

Q_SIGNALS:
    void mainWindowClosed();

protected:
    void closeEvent(QCloseEvent *event);

private Q_SLOTS:
    void addTab();
    void closeTab(int index);

    void itemActivated(uint resourceId);
    void handleMessage(const QString &message);

    void activate();
    void toggleFullScreen();
    void editPreferences();

    void appServerError(int error);
    void appServerAuthenticationRequired();

private:
    void destroyNavigator(CLLayoutNavigator *&nav);

private:
    QScopedPointer<QnBlueBackgroundPainter> m_backgroundPainter;
    QnWorkbenchController *m_controller;
    CLLayoutNavigator *m_normalView;
    QSplitter *m_splitter;
    TabWidget *m_tabWidget;

    static MainWnd *s_instance;
};

#endif // MAINWND_H
