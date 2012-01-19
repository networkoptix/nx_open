#ifndef MAINWND_H
#define MAINWND_H

#include <QWidget>
#include <QScopedPointer>

class QTabBar;
class QBoxLayout;
class QSpacerItem;

class QnBlueBackgroundPainter;
class QnGraphicsView;
class QnWorkbench;
class QnWorkbenchController;
class QnWorkbenchDisplay;
class QnWorkbenchLayout;
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
    void newLayout();
    void changeCurrentLayout(int index);
    void removeLayout(int index);

    void handleMessage(const QString &message);

    void treeWidgetItemActivated(uint resourceId);

    void openFile();

    void activate();
    void toggleFullScreen();

    void showAboutDialog();
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
    QTabBar *m_tabBar;

    QBoxLayout *m_titleLayout;
    QBoxLayout *m_viewLayout;
    QBoxLayout *m_globalLayout;

    bool m_titleVisible;

    QnDwm *m_dwm;
    bool m_drawCustomFrame;
};

#endif // MAINWND_H
