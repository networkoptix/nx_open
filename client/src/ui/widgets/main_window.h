#ifndef QN_MAIN_WINDOW_H
#define QN_MAIN_WINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <core/resource/resource_fwd.h>

class QTabBar;
class QBoxLayout;
class QSpacerItem;
class QToolButton;

class QnResourcePoolUserWatcher;
class QnBlueBackgroundPainter;
class QnLayoutTabBar;
class QnGraphicsView;
class QnDwm;
class QnWorkbench;
class QnWorkbenchController;
class QnWorkbenchUi;
class QnWorkbenchSynchronizer;
class QnWorkbenchDisplay;
class QnWorkbenchLayout;
class QnWorkbenchActionHandler;

class QnMainWindow : public QWidget
{
    Q_OBJECT;

    typedef QWidget base_type;

public:
    QnMainWindow(int argc, char* argv[], QWidget *parent = 0, Qt::WFlags flags = 0);

    virtual ~QnMainWindow();

    bool isTitleVisible() const {
        return m_titleVisible;
    }

protected:
    virtual bool event(QEvent *event) override;
    virtual void changeEvent(QEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;

#ifdef Q_OS_WIN
    virtual bool winEvent(MSG *message, long *result) override;
#endif

    bool canAutoDelete(const QnResourcePtr &resource) const;

protected slots:
    void setTitleVisible(bool visible);
    void setFullScreen(bool fullScreen);

    void toggleFullScreen();
    void toggleTitleVisibility();

    void showMainMenu();

    void handleMessage(const QString &message);

    void updateFullScreenState();
    void updateDwmState();

    void at_fileOpenSignalizer_activated(QObject *object, QEvent *event);

    void at_sessionManager_error(int error);

    void at_settings_lastUsedConnectionChanged();

    void at_synchronizer_started();

    void at_tabBar_closeRequested(QnWorkbenchLayout *layout);

private:
    QScopedPointer<QnBlueBackgroundPainter> m_backgroundPainter;
    QnWorkbenchController *m_controller;
    QnWorkbenchSynchronizer *m_synchronizer;
    QnWorkbenchUi *m_ui;
    QnWorkbenchDisplay *m_display;
    QnWorkbench *m_workbench;
    QnWorkbenchActionHandler *m_actionHandler;

    QnResourcePoolUserWatcher *m_userWatcher;

    QnGraphicsView *m_view;
    QnLayoutTabBar *m_tabBar;
    QToolButton *m_mainMenuButton;

    QBoxLayout *m_titleLayout;
    QBoxLayout *m_viewLayout;
    QBoxLayout *m_globalLayout;

    bool m_titleVisible;

    QnDwm *m_dwm;
    bool m_drawCustomFrame;
};

#endif // QN_MAIN_WINDOW_H
