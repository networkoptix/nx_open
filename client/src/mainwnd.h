#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QMainWindow>

class QTabWidget;
class QBoxLayout;
class QSpacerItem;

class QnBlueBackgroundPainter;
class QnGraphicsView;
class QnWorkbench;
class QnWorkbenchController;
class QnWorkbenchDisplay;
class QnDwm;

class MainWnd : public QWidget
{
    Q_OBJECT;

    typedef QWidget base_type;

public:
    MainWnd(int argc, char* argv[], QWidget *parent = 0, Qt::WFlags flags = 0);
    
    virtual ~MainWnd();

Q_SIGNALS:
    void mainWindowClosed();

protected:
    virtual void closeEvent(QCloseEvent *event) override;
    virtual void changeEvent(QEvent *event) override;

    virtual bool event(QEvent *event) override;

#ifdef Q_OS_WIN
    virtual bool winEvent(MSG *message, long *result) override;
#endif

private Q_SLOTS:
    void addTab();
    void currentTabChanged(int index);
    void closeTab(int index);

    void handleMessage(const QString &message);

    void activate();
    void toggleFullScreen();
    void editPreferences();

    void appServerError(int error);
    void appServerAuthenticationRequired();

    void toggleDecorationsVisibility();

    void updateDwmState();

private:
    QScopedPointer<QnBlueBackgroundPainter> m_backgroundPainter;
    QnWorkbenchController *m_controller;
    QnWorkbenchDisplay *m_display;
    QnWorkbench *m_workbench;
    QnGraphicsView *m_view;

    QTabWidget *m_tabWidget;
    QWidget *m_titleWidget;

    QSpacerItem *m_titleSpacer;
    QBoxLayout *m_titleLayout;
    QBoxLayout *m_viewLayout;

    QnDwm *m_dwm;
};

#endif // MAINWND_H
