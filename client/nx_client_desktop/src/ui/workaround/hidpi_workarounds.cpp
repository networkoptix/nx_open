#include "hidpi_workarounds.h"

#include <QtCore/QPointer>

#include <QtGui/QContextMenuEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QMovie>
#include <QtGui/QPixmap>
#include <QtGui/QScreen>
#include <QtGui/QWindow>

#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>

#include <nx/utils/raii_guard.h>

#include <utils/common/connective.h>

#include <ui/widgets/common/emulated_frame_widget.h>

namespace {

/*
 * Workaround geometry calculations when client is located on several screens with different DPI.
 * Geometry logic for Qt screens is following: screen position is given in pixel coordinates,
 * but screen size in it's own DPI-aware pixels. For example, 4K monitor to the left of main one
 * with DPI 2x will have geometry (-3840, 0, 1920*1080).
 */
QPoint screenRelatedToGlobal(const QPoint& point, QScreen* sourceScreen)
{
    const auto geometry = sourceScreen->geometry();
    if (geometry.contains(point))
        return point;

    const auto sourceDpi = sourceScreen->devicePixelRatio();
    const auto topLeft = geometry.topLeft(); //< Absolute position in pixels

    // This point may lay on other screen really. Position in DPI-pixels.
    auto scaledPointOnScreen = (point - topLeft);
    const auto globalPoint = (scaledPointOnScreen * sourceDpi) + topLeft; //< In global pixels.

    // Looking for the screen where point is really located.
    for (auto targetScreen: QGuiApplication::screens())
    {
        if (targetScreen == sourceScreen)
            continue;

        const auto targetGeometry = targetScreen->geometry();
        const auto dpi = targetScreen->devicePixelRatio();

        const auto dpiPointOnScreen = (globalPoint - targetGeometry.topLeft()) / dpi;
        if (targetGeometry.contains(dpiPointOnScreen))
            return dpiPointOnScreen;
    }

    return point;
}

QScreen* getScreen(const QPoint& scaled)
{
    const auto screens = QGuiApplication::screens();
    const auto it = std::find_if(screens.begin(), screens.end(),
        [scaled](const QScreen *screen)
        {
            const auto geometry = screen->geometry();
            return geometry.contains(scaled);
        });

    return (it == screens.end() ? nullptr : *it);
}

class MenuScreenCorrector: public Connective<QObject>
{
    typedef Connective<QObject> base_type;

public:
    MenuScreenCorrector(QMenu* parent):
        base_type(parent),
        m_screen(nullptr)
    {
    }

    void setTargetPosition(const QPoint& pos)
    {
        const auto newScreen = getScreen(pos);
        if (m_screen == newScreen)
            return;

        m_screen = newScreen;

        const auto window = getWindow();
        if (window)
            window->setScreen(m_screen);
    }

    virtual bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonRelease)
        {
            // Workarounds menu popup after second click in some cases.
            const auto mouseEvent = dynamic_cast<QMouseEvent*>(event);
            auto mouseMoveEvent = new QMouseEvent(QEvent::MouseMove, mouseEvent->localPos(),
                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            qApp->postEvent(watched, mouseMoveEvent);
            return QObject::eventFilter(watched, event);
        }

        if (!m_screen || (event->type() != QEvent::Show))
            return QObject::eventFilter(watched, event);

        const auto window = getWindow();
        if (!window)
            return QObject::eventFilter(watched, event);

        window->setScreen(m_screen);
        connect(window, &QWindow::screenChanged, this,
            [window, this](QScreen* newScreen)
            {
                if (window && (m_screen != newScreen))
                    window->setScreen(m_screen);
            });

        return QObject::eventFilter(watched, event);
    }

private:
    QPointer<QWindow> getWindow()
    {
        const auto menu = qobject_cast<QMenu*>(parent());
        QPointer<QWindow> window = menu->windowHandle();
        if (!window)
            return nullptr;

        while (window->parent())
            window = qobject_cast<QWindow*>(window->parent());
        return window;
    }
private:
    QScreen* m_screen;
};

typedef QSharedPointer<MenuScreenCorrector> MenuScreenCorrectorPtr;
typedef QHash<QMenu*, MenuScreenCorrectorPtr> MenuScreenCorrectorsHash;

MenuScreenCorrectorPtr installMenuMouseEventCorrector(QMenu* menu)
{
    static MenuScreenCorrectorsHash knownCorrectors;
    const auto itCorrector = knownCorrectors.find(menu);
    if (itCorrector != knownCorrectors.end())
        return *itCorrector;

    const auto corrector = MenuScreenCorrectorPtr(new MenuScreenCorrector(menu));
    knownCorrectors.insert(menu, corrector);
    menu->connect(menu, &QObject::destroyed, menu,
        [menu]() { knownCorrectors.remove(menu); });

    menu->installEventFilter(corrector.data());
    return corrector;
}

QWindow* getParentWindow(QWidget* widget)
{
    if (!widget)
        return nullptr;

    auto topLevel = widget->window();
    if (topLevel->isWindow())
        return topLevel->windowHandle();

    const auto proxy = topLevel->graphicsProxyWidget();
    if (!proxy)
        return nullptr;

    const auto scene = proxy->scene();
    if (!scene || scene->views().isEmpty())
        return nullptr;

    return getParentWindow(scene->views().first());
}

QPoint getPoint(QWidget* widget, const QPoint& offset, QWindow* parentWindow = nullptr)
{
    if (!parentWindow)
        parentWindow = getParentWindow(widget);

    if (!parentWindow)
        return QPoint();

    const QPoint globalPos = widget->mapToGlobal(offset);
    return screenRelatedToGlobal(globalPos, parentWindow->screen());
}

class TopLevelWidgetsPositionCorrector : public QObject
{
public:
    bool eventFilter(QObject*watched, QEvent* event)
    {
        if (event->type() != QEvent::Show)
            return QObject::eventFilter(watched, event);

        QWidget* properWidget = qobject_cast<QnEmulatedFrameWidget*>(watched); //< Main window.
        if (!properWidget)
            properWidget = qobject_cast<QDialog*>(watched); //< Any dialog.
        if (!properWidget)
            return QObject::eventFilter(watched, event);

        const auto parentWindow = getParentWindow(properWidget);
        NX_EXPECT(parentWindow);
        const auto geometry = properWidget->geometry();
        const auto globalPos = parentWindow->geometry().topLeft();
        const auto fixedPos = screenRelatedToGlobal(globalPos, parentWindow->screen());
        parentWindow->setScreen(getScreen(fixedPos));
        parentWindow->setPosition(fixedPos);
        return QObject::eventFilter(watched, event);
    }
};

class ContextMenuEventCorrector : public QObject
{
    class ProxyContextMenuEvent : public QContextMenuEvent
    {
    public:
        ProxyContextMenuEvent(const QPoint& pos, const QPoint& globalPos):
            QContextMenuEvent(QContextMenuEvent::Mouse,   //< Always set mouse to prevent coordinates transforming
                pos, globalPos)
        {}
    };

public:
    bool eventFilter(QObject* watched, QEvent* event)
    {
        //if (event->type() != QEvent::ContextMenu)
            return QObject::eventFilter(watched, event);

        const auto contextMenuEvent = static_cast<QContextMenuEvent*>(event);
        const auto widget = dynamic_cast<QWidget*>(watched);
        const auto parentWindow = getParentWindow(widget);
        const auto nativeEvent = (event->spontaneous()
            && !dynamic_cast<ProxyContextMenuEvent*>(event));

        if (nativeEvent && parentWindow)
        {
            const auto globalPos = contextMenuEvent->globalPos();
            const auto targetPos = screenRelatedToGlobal(globalPos, parentWindow->screen());
            auto fixedEvent = ProxyContextMenuEvent(contextMenuEvent->pos(), targetPos);
            if (!qApp->sendEvent(watched, &fixedEvent) || !fixedEvent.isAccepted())
                return QObject::eventFilter(watched, event);

            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

static bool initializedWorkarounds = false;
static bool isWindowsEnvironment =
    #if defined(Q_OS_WIN)
    true;
    #else
    false;
    #endif

}   // namespace

QAction* QnHiDpiWorkarounds::showMenu(QMenu* menu, const QPoint& globalPoint)
{
    if (isWindowsEnvironment)
    {
        const auto corrector = installMenuMouseEventCorrector(menu);
        corrector->setTargetPosition(globalPoint);
    }

    return menu->exec(globalPoint);
}

QPoint QnHiDpiWorkarounds::safeMapToGlobal(QWidget* widget, const QPoint& offset)
{
    if (isWindowsEnvironment)
        return getPoint(widget, offset);

    return widget->mapToGlobal(offset);
}

void QnHiDpiWorkarounds::init()
{
    if (!isWindowsEnvironment || initializedWorkarounds)
        return;

    initializedWorkarounds = true;
    qApp->installEventFilter(new ContextMenuEventCorrector());
    qApp->installEventFilter(new TopLevelWidgetsPositionCorrector());
}

void QnHiDpiWorkarounds::setMovieToLabel(QLabel* label, QMovie* movie)
{
    if (!label || !movie)
        return;

    const bool started = movie->state() != QMovie::NotRunning;
    QnRaiiGuardPtr stopGuard;
    if (!started)
    {
        stopGuard = QnRaiiGuard::createDestructible([movie](){ movie->stop(); });
        movie->start();
    }

    const auto pixmap = movie->currentPixmap();
    label->setMovie(movie);
    if (pixmap.isNull())
        return;

    const auto fixedSize = pixmap.size() / pixmap.devicePixelRatio();
    label->setFixedSize(fixedSize);
}
