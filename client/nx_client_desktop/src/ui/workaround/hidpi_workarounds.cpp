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

typedef QMultiMap<int, QScreen*> SortedScreens;

// TODO: #ynikitenkov develop totally correct function -
// now it correct only for one horizontal line positioned screen
QPoint screenRelatedToGlobal(const QPoint& point, QScreen* screen)
{
    const auto geometry = screen->geometry();
    if (geometry.contains(point))
        return point;

    const auto topLeft = geometry.topLeft();
    const auto left = topLeft.x();
    auto scaledPointOnScreen = (point - topLeft);

    const bool backwardSearch = ((scaledPointOnScreen.x() < 0)
        || (scaledPointOnScreen.y() < 0));

    if (!backwardSearch)
        scaledPointOnScreen -= QPoint(geometry.width(), 0);

    const auto pixelPointOnScreen = scaledPointOnScreen * screen->devicePixelRatio();
    SortedScreens tmp;
    for (auto src : QGuiApplication::screens())
        tmp.insert(src->geometry().left(), src);

    // Searching for appropriate screen
    const auto xSorted = tmp.values().toVector();
    const auto it = std::find_if(xSorted.begin(), xSorted.end(),
        [screen](const QScreen* value)
        {
            return (value == screen);
        });

    if (it == xSorted.end())
        return point;

    const auto itRealScreen = (backwardSearch
        ? (it == xSorted.begin() ? xSorted.end() : it - 1)
        : (it == (xSorted.end() - 1) ? xSorted.end() : it + 1));

    if (itRealScreen == xSorted.end())
        return QPoint();

    // We've found prev screen
    QScreen *appropriate = *itRealScreen;
    const auto targetGeometry = appropriate->geometry();
    const auto offset = (backwardSearch ? QPoint(targetGeometry.width(), 0) : QPoint());
    const auto factor = appropriate->devicePixelRatio();
    const auto targetTopLeft = targetGeometry.topLeft();

    const auto result = targetTopLeft + offset + pixelPointOnScreen / factor;;
    if (appropriate->geometry().contains(result))
        return result;

    // Looking for next screen
    return screenRelatedToGlobal(result, appropriate);
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

        QWidget* properWidget = dynamic_cast<QnEmulatedFrameWidget*>(watched);  // Main window
        if (!properWidget)
            properWidget = dynamic_cast<QDialog*>(watched); // Any dialog
        if (!properWidget)
            return QObject::eventFilter(watched, event);

        const auto parentWindow = getParentWindow(properWidget);
        const auto geometry = properWidget->geometry();
        const auto fixedPos = screenRelatedToGlobal(
            parentWindow->geometry().topLeft(), parentWindow->screen());
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
            const auto targetPos = screenRelatedToGlobal(
                contextMenuEvent->globalPos(), parentWindow->screen());
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
