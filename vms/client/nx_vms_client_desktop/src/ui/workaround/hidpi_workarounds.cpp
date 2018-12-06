#include "hidpi_workarounds.h"

#include <cmath>

#include <QtCore/QPointer>

#include <QtGui/QGuiApplication>
#include <QtGui/QMovie>
#include <QtGui/QPixmap>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtGui/QMouseEvent>
#include <QtGui/QEnterEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QContextMenuEvent>

#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsSceneHoverEvent>
#include <QtWidgets/QGraphicsSceneWheelEvent>
#include <QtWidgets/QGraphicsSceneContextMenuEvent>
#include <QtWidgets/QGraphicsSceneDragDropEvent>

#include <nx/utils/scope_guard.h>

#include <utils/common/connective.h>

#include <ui/widgets/common/emulated_frame_widget.h>

namespace {

/*
 * Workaround geometry calculations when client is located on several screens with different DPI.
 * Geometry logic for Qt screens is following: screen position is given in device coordinates,
 * but screen size is in logical coordinates. For example, 4K monitor to the left of main one
 * with DPI 2x will have geometry (-3840, 0, 1920*1080).
 */
QPoint screenRelatedToGlobal(const QPoint& point, QScreen* sourceScreen)
{
    if (!sourceScreen)
        return point;

    const auto geometry = sourceScreen->geometry();
    if (geometry.contains(point))
        return point;

    const auto sourceDpi = sourceScreen->devicePixelRatio();
    const auto topLeft = geometry.topLeft(); //< Absolute position in pixels

    // This point may lay on other screen really. Position in logical coordinates using dpi from
    // the screen where the biggest part of the client window is located.
    auto scaledPointOnScreen = (point - topLeft);
    const auto globalPoint = (scaledPointOnScreen * sourceDpi) + topLeft; //< Device coordinates.

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
    QPointer<QScreen> m_screen;
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

QWindow* getParentWindow(const QWidget* widget)
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

QPoint getPoint(const QWidget* widget, const QPoint& offset, QWindow* parentWindow = nullptr)
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
        NX_ASSERT(parentWindow);
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
        if (event->type() != QEvent::ContextMenu)
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

QPoint fixupScreenPos(QWidget* viewport, const QPointF& scenePos, const QPoint& fallbackScreenPos)
{
    if (!viewport)
        return fallbackScreenPos;

    const auto view = qobject_cast<QGraphicsView*>(viewport->parent());
    if (!view || !view->scene())
        return fallbackScreenPos;

    return view->mapToGlobal(view->mapFromScene(scenePos));
}

qreal frac(qreal source)
{
    return source - std::trunc(source);
}

QPointF frac(const QPointF& source)
{
    return QPointF(frac(source.x()), frac(source.y()));
}

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

QPoint QnHiDpiWorkarounds::safeMapToGlobal(const QWidget* widget, const QPoint& offset)
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
    nx::utils::Guard stopGuard;
    if (!started)
    {
        stopGuard = nx::utils::Guard([movie](){ movie->stop(); });
        movie->start();
    }

    const auto pixmap = movie->currentPixmap();
    label->setMovie(movie);
    if (pixmap.isNull())
        return;

    const auto fixedSize = pixmap.size() / pixmap.devicePixelRatio();
    label->setFixedSize(fixedSize);
}

bool QnHiDpiWorkarounds::fixupGraphicsSceneEvent(QEvent* event)
{
    if (!event)
        return false;

    switch (event->type())
    {
        case QEvent::GraphicsSceneMouseMove:
        case QEvent::GraphicsSceneMousePress:
        case QEvent::GraphicsSceneMouseRelease:
        case QEvent::GraphicsSceneMouseDoubleClick:
        {
            auto mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
            mouseEvent->setScreenPos(fixupScreenPos(
                mouseEvent->widget(), mouseEvent->scenePos(), mouseEvent->screenPos()));
            mouseEvent->setLastScreenPos(fixupScreenPos(
                mouseEvent->widget(), mouseEvent->lastScenePos(), mouseEvent->lastScreenPos()));
            return true;
        }

        case QEvent::GraphicsSceneHoverEnter:
        case QEvent::GraphicsSceneHoverLeave:
        case QEvent::GraphicsSceneHoverMove:
        {
            auto hoverEvent = static_cast<QGraphicsSceneHoverEvent*>(event);
            hoverEvent->setScreenPos(fixupScreenPos(
                hoverEvent->widget(), hoverEvent->scenePos(), hoverEvent->screenPos()));
            hoverEvent->setLastScreenPos(fixupScreenPos(
                hoverEvent->widget(), hoverEvent->lastScenePos(), hoverEvent->lastScreenPos()));
            return true;
        }

        case QEvent::GraphicsSceneContextMenu:
        {
            auto menuEvent = static_cast<QGraphicsSceneContextMenuEvent*>(event);
            menuEvent->setScreenPos(fixupScreenPos(
                menuEvent->widget(), menuEvent->scenePos(), menuEvent->screenPos()));
            return true;
        }

        case QEvent::GraphicsSceneDragEnter:
        case QEvent::GraphicsSceneDragLeave:
        case QEvent::GraphicsSceneDragMove:
        case QEvent::GraphicsSceneDrop:
        {
            auto dndEvent = static_cast<QGraphicsSceneDragDropEvent*>(event);
            dndEvent->setScreenPos(fixupScreenPos(
                dndEvent->widget(), dndEvent->scenePos(), dndEvent->screenPos()));
            return true;
        }

        case QEvent::GraphicsSceneWheel:
        {
            auto wheelEvent = static_cast<QGraphicsSceneWheelEvent*>(event);
            wheelEvent->setScreenPos(fixupScreenPos(
                wheelEvent->widget(), wheelEvent->scenePos(), wheelEvent->screenPos()));
            return true;
        }

        default:
            return false;
    }
}

QEvent* QnHiDpiWorkarounds::fixupEvent(
    QWidget* widget, QEvent* source, std::unique_ptr<QEvent>& target)
{
    if (!source)
        return nullptr;

    // First, handle Graphics Scene events. They are fixed up in place, without making a copy.

    if (fixupGraphicsSceneEvent(source))
        return source;

    // Second, handle other events. They are fixed as a copy, but losing spontaneous flag.

    if (!widget)
        return source;

    switch (source->type())
    {
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        {
            const auto& mouseEvent = *static_cast<QMouseEvent*>(source);
            target.reset(new QMouseEvent(
                source->type(),
                mouseEvent.localPos(),
                mouseEvent.windowPos(),
                widget->mapToGlobal(mouseEvent.pos()) + frac(mouseEvent.screenPos()),
                mouseEvent.button(),
                mouseEvent.buttons(),
                mouseEvent.modifiers(),
                mouseEvent.source()));
            target->setAccepted(source->isAccepted());
            return target.get();
        }

        case QEvent::Enter:
        {
            const auto& enterEvent = *static_cast<QEnterEvent*>(source);
            target.reset(new QEnterEvent(
                enterEvent.localPos(),
                enterEvent.windowPos(),
                widget->mapToGlobal(enterEvent.pos()) + frac(enterEvent.screenPos())));
            target->setAccepted(source->isAccepted());
            return target.get();
        }

        case QEvent::Wheel:
        {
            const auto& wheelEvent = *static_cast<QWheelEvent*>(source);
            target.reset(new QWheelEvent(
                wheelEvent.posF(),
                widget->mapToGlobal(wheelEvent.pos()) + frac(wheelEvent.globalPosF()),
                wheelEvent.pixelDelta(),
                wheelEvent.angleDelta(),
                wheelEvent.delta(),
                wheelEvent.orientation(),
                wheelEvent.buttons(),
                wheelEvent.modifiers(),
                wheelEvent.phase(),
                wheelEvent.source(),
                wheelEvent.inverted()));
            target->setAccepted(source->isAccepted());
            return target.get();
        }

        case QEvent::ContextMenu:
        {
            const auto& menuEvent = *static_cast<QContextMenuEvent*>(source);
            target.reset(new QContextMenuEvent(
                menuEvent.reason(),
                menuEvent.pos(),
                widget->mapToGlobal(menuEvent.pos()),
                menuEvent.modifiers()));
            target->setAccepted(source->isAccepted());
            return target.get();
        }

        default:
            return source;
    }
}
