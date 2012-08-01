#ifndef QN_MAIN_WINDOW_H
#define QN_MAIN_WINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <core/resource/resource_fwd.h>
#include <ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/processors/drag_process_handler.h>

class QTabBar;
class QBoxLayout;
class QSpacerItem;
class QToolButton;

class QnActionManager;
class QnResourcePoolUserWatcher;
class QnGradientBackgroundPainter;
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

class QnMainWindow: public QWidget, public QnWorkbenchContextAware, public DragProcessHandler {
    Q_OBJECT;

    typedef QWidget base_type;

public:
    enum Option {
        TitleBarDraggable = 0x1,    /**< Window can be moved by dragging the title bar. */
        WindowButtonsVisible = 0x2, /**< Window has default title bar buttons. That is, close, maximize and minimize buttons. */
    };
    Q_DECLARE_FLAGS(Options, Option);

    QnMainWindow(QnWorkbenchContext *context, QWidget *parent = 0, Qt::WFlags flags = 0);
    virtual ~QnMainWindow();

    bool isTitleVisible() const {
        return m_titleVisible;
    }

    Options options() const;
    void setOptions(Options options);

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
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;

    virtual void dragMove(DragInfo *info) override;

#ifdef Q_OS_WIN
    virtual bool winEvent(MSG *message, long *result) override;
#endif

    bool isTabBar(const QPoint &pos) const;

protected slots:
    void setTitleVisible(bool visible);
    void setWindowButtonsVisible(bool visible);
    void setFullScreen(bool fullScreen);
    void minimize();

    void toggleFullScreen();
    void toggleTitleVisibility();

    void updateFullScreenState();
    void updateDwmState();
    void updateTitleBarDraggable();

    void at_fileOpenSignalizer_activated(QObject *object, QEvent *event);
    void at_sessionManager_error(int error);
    void at_tabBar_closeRequested(QnWorkbenchLayout *layout);
    void at_mainMenuAction_triggered();

    void test() {
        setOptions(m_options ^ WindowButtonsVisible);
    }

private:
    QScopedPointer<QnGradientBackgroundPainter> m_backgroundPainter;
    QnWorkbenchController *m_controller;
    QnWorkbenchUi *m_ui;
    QnWorkbenchActionHandler *m_actionHandler;

    QnGraphicsView *m_view;
    QnLayoutTabBar *m_tabBar;
    QToolButton *m_mainMenuButton;

    QBoxLayout *m_titleLayout;
    QBoxLayout *m_windowButtonsLayout;
    QBoxLayout *m_viewLayout;
    QBoxLayout *m_globalLayout;

    bool m_titleVisible;

    QnResourceList m_dropResources;

    QnDwm *m_dwm;
    bool m_drawCustomFrame;

    Options m_options;

    DragProcessor *m_dragProcessor;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnMainWindow::Options);

#endif // QN_MAIN_WINDOW_H
