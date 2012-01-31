#ifndef MAINWND_H
#define MAINWND_H

#include <QWidget>
#include <QScopedPointer>

class QTabBar;
class QBoxLayout;
class QSpacerItem;

class QnBlueBackgroundPainter;
class QnLayoutTabBar;
class QnGraphicsView;
class QnDwm;
class QnWorkbench;
class QnWorkbenchController;
class QnWorkbenchUi;
class QnWorkbenchDisplay;
class QnWorkbenchLayout;

class MainWnd : public QWidget
{
    Q_OBJECT;

    typedef QWidget base_type;

public:
    MainWnd(int argc, char* argv[], QWidget *parent = 0, Qt::WFlags flags = 0);

    virtual ~MainWnd();

    bool isTitleVisible() const {
        return m_titleVisible;
    }

Q_SIGNALS:
    void mainWindowClosed();

protected:
    virtual bool event(QEvent *event) override;
    virtual void closeEvent(QCloseEvent *event) override;
    virtual void changeEvent(QEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;

#ifdef Q_OS_WIN
    virtual bool winEvent(MSG *message, long *result) override;
#endif

protected Q_SLOTS:
    void setTitleVisible(bool visible);
    void setFullScreen(bool fullScreen);

    void toggleFullScreen();
    void toggleTitleVisibility();

    void handleMessage(const QString &message);

    void showOpenFileDialog();
    void showAboutDialog();
    void showPreferencesDialog();
    void showAuthenticationDialog();

    void at_sessionManager_error(int error);

    void updateFullScreenState();
    void updateDwmState();

    void at_newLayoutRequested();
    void at_tabBar_layoutRemoved(QnWorkbenchLayout *layout);
    void at_tabBar_currentChanged(QnWorkbenchLayout *layout);
    void at_treeWidget_activated(uint resourceId);


private:
    QScopedPointer<QnBlueBackgroundPainter> m_backgroundPainter;
    QnWorkbenchController *m_controller;
    QnWorkbenchUi *m_ui;
    QnWorkbenchDisplay *m_display;
    QnWorkbench *m_workbench;

    QnGraphicsView *m_view;
    QnLayoutTabBar *m_tabBar;

    QBoxLayout *m_titleLayout;
    QBoxLayout *m_viewLayout;
    QBoxLayout *m_globalLayout;

    bool m_titleVisible;

    QnDwm *m_dwm;
    bool m_drawCustomFrame;
};

#endif // MAINWND_H
