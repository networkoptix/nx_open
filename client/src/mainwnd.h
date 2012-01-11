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
    virtual void paintEvent(QPaintEvent *event) override;

#ifdef Q_OS_WIN
    virtual bool winEvent(MSG *message, long *result) override;
#endif

protected Q_SLOTS:
    void addTab();
    void currentTabChanged(int index);
    void closeTab(int index);

    void handleMessage(const QString &message);

    void activate();
    void toggleFullScreen();
    void editPreferences();

    void appServerError(int error);
    void appServerAuthenticationRequired();

    void toggleTitleVisibility();

    void updateDwmState();

protected:
    bool isTitleVisible() const;

private:
    QScopedPointer<QnBlueBackgroundPainter> m_backgroundPainter;
    QnWorkbenchController *m_controller;
    QnWorkbenchDisplay *m_display;
    QnWorkbench *m_workbench;
    QnGraphicsView *m_view;

    QTabWidget *m_tabWidget;

    QSpacerItem *m_titleSpacer;
    QBoxLayout *m_titleLayout;
    QBoxLayout *m_viewLayout;
    QBoxLayout *m_globalLayout;

    QnDwm *m_dwm;
    bool m_drawCustomFrame;
};

#endif // MAINWND_H
