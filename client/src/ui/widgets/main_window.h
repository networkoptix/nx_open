#ifndef QN_MAIN_WINDOW_H
#define QN_MAIN_WINDOW_H

#include <QWidget>
#include <QScopedPointer>
#include <core/resource/resource_fwd.h>
#include <ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>
#include "emulated_frame_widget.h"

class QTabBar;
class QBoxLayout;
class QSpacerItem;
class QToolButton;

class QnActionManager;
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

class QnMainWindow: public QnEmulatedFrameWidget, public QnWorkbenchContextAware {
    Q_OBJECT;

    typedef QnEmulatedFrameWidget base_type;

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
    virtual void dragEnterEvent(QDragEnterEvent *event) override;
    virtual void dragMoveEvent(QDragMoveEvent *event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
    virtual void dropEvent(QDropEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;

    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPoint &pos) const override;

#ifdef Q_OS_WIN
    virtual bool winEvent(MSG *message, long *result) override;
#endif

protected slots:
    void setTitleVisible(bool visible);
    void setWindowButtonsVisible(bool visible);
    void setMaximized(bool maximized);
    void setFullScreen(bool fullScreen);
    void minimize();

    void toggleFullScreen();
    void toggleTitleVisibility();

    void updateDecorationsState();
    void updateDwmState();

    void at_fileOpenSignalizer_activated(QObject *object, QEvent *event);
    void at_tabBar_closeRequested(QnWorkbenchLayout *layout);

private:
    QScopedPointer<QnGradientBackgroundPainter> m_backgroundPainter;
    QnWorkbenchController *m_controller;
    QnWorkbenchUi *m_ui;

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
    QMargins m_frameMargins;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnMainWindow::Options);

#endif // QN_MAIN_WINDOW_H
