#ifndef QN_MAIN_WINDOW_H
#define QN_MAIN_WINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <core/resource/resource_fwd.h>
#include <ui/actions/actions.h>

class QTabBar;
class QBoxLayout;
class QSpacerItem;
class QToolButton;

class QnActionManager;
class QnResourcePoolUserWatcher;
class QnBlueBackgroundPainter;
class QnLayoutTabBar;
class QnGraphicsView;
class QnDwm;
class QnWorkbench;
class QnWorkbenchContext;
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
    QnMainWindow(QWidget *parent = 0, Qt::WFlags flags = 0);

    virtual ~QnMainWindow();

    bool isTitleVisible() const {
        return m_titleVisible;
    }

    QnWorkbenchContext *context() const {
        return m_context;
    }

public slots:
    void handleMessage(const QString &message);

protected:
    virtual bool event(QEvent *event) override;
    virtual void changeEvent(QEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void dragEnterEvent(QDragEnterEvent *event) override;
    virtual void dragMoveEvent(QDragMoveEvent *event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
    virtual void dropEvent(QDropEvent *event) override;

#ifdef Q_OS_WIN
    virtual bool winEvent(MSG *message, long *result) override;
#endif

    bool canAutoDelete(const QnResourcePtr &resource) const;

    QAction *action(const Qn::ActionId id) const;

    QnActionManager *menu() const;

protected slots:
    void setTitleVisible(bool visible);
    void setFullScreen(bool fullScreen);
    void minimize();

    void toggleFullScreen();
    void toggleTitleVisibility();

    void updateFullScreenState();
    void updateDwmState();

    void at_fileOpenSignalizer_activated(QObject *object, QEvent *event);
    void at_sessionManager_error(int error);
    void at_tabBar_closeRequested(QnWorkbenchLayout *layout);
    void at_mainMenuAction_triggered();

private:
    QScopedPointer<QnBlueBackgroundPainter> m_backgroundPainter;
    QnWorkbenchContext *m_context;
    QnWorkbenchController *m_controller;
    QnWorkbenchUi *m_ui;
    QnWorkbenchDisplay *m_display;
    QnWorkbenchActionHandler *m_actionHandler;

    QnGraphicsView *m_view;
    QnLayoutTabBar *m_tabBar;
    QToolButton *m_mainMenuButton;

    QBoxLayout *m_titleLayout;
    QBoxLayout *m_viewLayout;
    QBoxLayout *m_globalLayout;

    bool m_titleVisible;

    QnResourceList m_dropResources;

    QnDwm *m_dwm;
    bool m_drawCustomFrame;
};

#endif // QN_MAIN_WINDOW_H
