#ifndef QN_MAIN_WINDOW_H
#define QN_MAIN_WINDOW_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QMainWindow>
#include <QtCore/QScopedPointer>
#include <core/resource/resource_fwd.h>
#include <ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/graphics/view/graphics_scene.h>

#include <ui/widgets/common/emulated_frame_widget.h>

class QTabBar;
class QBoxLayout;
class QSpacerItem;
class QToolButton;
class QStackedWidget;

class QnActionManager;
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
class QnMainWindowTitleBarWidget;

class QnMainWindow: public QnEmulatedFrameWidget, public QnWorkbenchContextAware {
    Q_OBJECT;

    typedef QnEmulatedFrameWidget base_type;

public:
    enum Option {
        TitleBarDraggable = 0x1,    /**< Window can be moved by dragging the title bar. */
    };
    Q_DECLARE_FLAGS(Options, Option);

    QnMainWindow(QnWorkbenchContext *context, QWidget *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~QnMainWindow();

    bool isTitleVisible() const;

    Options options() const;
    void setOptions(Options options);

    void setAnimationsEnabled(bool enabled = true);

    QWidget *viewport() const;

    void updateDecorationsState();
public slots:
    bool handleMessage(const QString &message);

protected:
    virtual bool event(QEvent *event) override;
    virtual void closeEvent(QCloseEvent* event) override;
    virtual void changeEvent(QEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void moveEvent(QMoveEvent *event) override;

    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPoint &pos) const override;

    virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

protected slots:
    void setTitleVisible(bool visible);
    void setMaximized(bool maximized);
    void setFullScreen(bool fullScreen);
    void minimize();

    void updateDwmState();

    void at_fileOpenSignalizer_activated(QObject *object, QEvent *event);

private:
    bool isWelcomeScreenVisible() const;

    void updateWidgetsVisibility();

    void showFullScreen();
    void showNormal();

    void updateScreenInfo();

    void updateHelpTopic();

private:
    QnDwm *m_dwm;

    /* Note that destruction order is important here, so we use scoped pointers. */
    QScopedPointer<QnGraphicsView> m_view;
    QScopedPointer<QnGraphicsScene> m_scene;
    QScopedPointer<QnWorkbenchController> m_controller;
    QScopedPointer<QnWorkbenchUi> m_ui;

    QStackedWidget * const m_currentPageHolder;

    QnMainWindowTitleBarWidget *m_titleBar;
    QBoxLayout *m_viewLayout;
    QBoxLayout *m_globalLayout;

    bool m_titleVisible;

    bool m_drawCustomFrame;

    Options m_options;
    QMargins m_frameMargins;

#ifndef Q_OS_MACX
    /** This field is used to restore geometry after switching to fullscreen and back. Do not used in MacOsX due to its fullscreen mode. */
    QRect m_storedGeometry;
#endif

    bool m_inFullscreenTransition;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnMainWindow::Options);

#endif // QN_MAIN_WINDOW_H
