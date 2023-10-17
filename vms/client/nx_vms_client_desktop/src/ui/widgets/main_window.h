// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/widgets/common/emulated_frame_widget.h>

class QBoxLayout;
class QStackedLayout;

class QnGraphicsScene;
class QnGraphicsView;
class QnWorkbenchController;
class QnMainWindowTitleBarWidget;

namespace nx::vms::client::desktop {

class WorkbenchUi;
class ScreenManager;
class WelcomeScreen;
class MainWindowTitleBarStateStore;

class MainWindow: public QnEmulatedFrameWidget, public WindowContextAware
{
    Q_OBJECT

    using base_type = QnEmulatedFrameWidget;

public:
    enum Option {
        TitleBarDraggable = 0x1,    /**< Window can be moved by dragging the title bar. */
    };
    Q_DECLARE_FLAGS(Options, Option)

    MainWindow(
        WindowContext *context,
        QWidget *parent = nullptr,
        Qt::WindowFlags flags = Qt::Window);

    virtual ~MainWindow() override;

    QGraphicsView* view() const;

    QGraphicsScene* scene() const;

    bool isTitleVisible() const;

    Options options() const;
    void setOptions(Options options);

    void setAnimationsEnabled(bool enabled = true);

    QWidget* viewport() const;
    ScreenManager* screenManager() const;

    WelcomeScreen* welcomeScreen() const;
    void setWelcomeScreenVisible(bool visible);

    QnMainWindowTitleBarWidget* titleBar() const;

    void updateDecorationsState();

    bool isWorkbenchVisible() const;
    bool isWelcomeScreenVisible() const;

    /**
     * Handle key press.
     * @returns true if key was handled.
     */
    bool handleKeyPress(int key);

    bool isSystemTabBarUpdating();

public slots:
    bool handleOpenFile(const QString &message);

protected:
    virtual bool event(QEvent *event) override;
    virtual void closeEvent(QCloseEvent* event) override;
    virtual void changeEvent(QEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void moveEvent(QMoveEvent *event) override;

    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPoint &pos) const override;

#ifdef Q_OS_WIN
    virtual bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif // Q_OS_WIN

protected slots:
    void setTitleVisible(bool visible);
    void setMaximized(bool maximized);
    void setFullScreen(bool fullScreen);
    void minimize();

    void at_fileOpenSignalizer_activated(QObject* object, QEvent* event);

private:
    void updateWidgetsVisibility();

    void showFullScreen();
    void showNormal();

    void updateScreenInfo();
    void updateContentsMargins();

    std::pair<int, bool> calculateHelpTopic() const;
    void updateHelpTopic();

    bool isFullScreenMode() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    /* Note that destruction order is important here, so we use scoped pointers. */
    QScopedPointer<QnGraphicsView> m_view;
    QScopedPointer<QnGraphicsScene> m_scene;
    QScopedPointer<QnWorkbenchController> m_controller;
    QScopedPointer<WorkbenchUi> m_ui;
    WelcomeScreen* m_welcomeScreen = nullptr;

    QnMainWindowTitleBarWidget* m_titleBar = nullptr;
    QStackedLayout* m_viewLayout = nullptr;
    QBoxLayout* m_globalLayout = nullptr;

    bool m_welcomeScreenVisible = false;
    bool m_titleVisible = true;

    bool m_drawCustomFrame = false;

    Options m_options;
    QMargins m_frameMargins;

    bool m_inFullscreen = false;
    bool m_inFullscreenTransition = false;
    bool m_initialized = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MainWindow::Options);

} // namespace nx::vms::client::desktop
