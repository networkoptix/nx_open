
#include "hidpi_workarounds.h"

#include <nx/utils/raii_guard.h>
#include <utils/common/connective.h>

namespace {

typedef QMultiMap<int, QScreen*> SortedScreens;
QPoint convertScaledToGlobal(const QPoint& scaled)
{

    QPoint result(scaled);

    SortedScreens xSorted;
    for (auto screen : QGuiApplication::screens())
        xSorted.insert(screen->geometry().left(), screen);

    QPoint logicalOffset;
    for (auto it = xSorted.begin(); it != xSorted.end(); ++it)
    {
        const auto geometry = it.value()->geometry();
        const auto offset = QPoint(geometry.width(), 0);
        const auto newValue = logicalOffset + offset;
        if (result.x() > newValue.x())
        {
            logicalOffset += offset;
            continue;
        }

        const auto screenOrigin = geometry.topLeft();
        return screenOrigin + (result - logicalOffset);
    }

    return result;
}

// TODO: #ynikitenkov develop totally correct functiom -
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

    if (it == xSorted.end())
        return QPoint();

    // We've found prev screen
    QScreen *appropriate = *itRealScreen;
    const auto targetGeometry = appropriate->geometry();
    const auto offset = (backwardSearch ? QPoint(targetGeometry.width(), 0) : QPoint());
    const auto factor = appropriate->devicePixelRatio();
    const auto targetTopLeft = targetGeometry.topLeft();
    return targetTopLeft + offset + pixelPointOnScreen / factor;
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

class MenuMouseEventsCorrector: public Connective<QObject>
{
    typedef Connective<QObject> base_type;

public:
    MenuMouseEventsCorrector(QMenu* parent):
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

typedef QSharedPointer<MenuMouseEventsCorrector> MouseEventsCorrectorPtr;
typedef QHash<QMenu*, MouseEventsCorrectorPtr> MouseEventsCorrectorsHash;

static MouseEventsCorrectorsHash knownCorrectors;

static bool customMenuIsVisible = false;

void installMenuMouseEventCorrector(QMenu* menu)
{
    if (knownCorrectors.contains(menu))
        return;

    const auto corrector = MouseEventsCorrectorPtr(new MenuMouseEventsCorrector(menu));
    knownCorrectors.insert(menu, corrector);
    menu->connect(menu, &QObject::destroyed, menu,
        [menu]() { knownCorrectors.remove(menu); });

    menu->installEventFilter(corrector.data());
}

QWindow* getParentWindow(QWidget* widget)
{
    // Checks if widget is in graphics view

    if (!widget)
        return nullptr;

    auto item = widget;
    while (item->parentWidget() && !dynamic_cast<QGLWidget*>(item->parentWidget()))
        item = item->parentWidget();

    if (item->parentWidget())    //< direct child of GL widget
        return item->parentWidget()->windowHandle();

    // We have at least
    const auto proxy = item->graphicsProxyWidget();
    const QGraphicsView* view = (proxy && proxy->scene() && !proxy->scene()->views().isEmpty()
        ? proxy->scene()->views().first() : nullptr);
    if (view)
        return view->viewport()->windowHandle();

    while (widget)
    {
        if (widget->windowHandle())
            return widget->windowHandle();

        widget = widget->parentWidget();
    }
    return nullptr;
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

class CE : public QContextMenuEvent
{
public:
    CE(QContextMenuEvent::Reason reason, const QPoint& pos, const QPoint& globalPos)
        :QContextMenuEvent(QContextMenuEvent::Mouse,   //< Always set mouse to prevent coordinates transforming
            pos, globalPos)
    {}
};

class Test : public QObject
{
private:
    bool eventFilter(QObject*watched, QEvent* event)
    {
        if (event->type() != QEvent::ContextMenu)
            return QObject::eventFilter(watched, event);

        const auto contextMenuEvent = dynamic_cast<QContextMenuEvent*>(event);
        const auto widget = qobject_cast<QWidget*>(watched);
        const auto parentWindow = getParentWindow(widget);
        if (!dynamic_cast<CE*>(event) && parentWindow && !customMenuIsVisible)   // Ignore events from directly shown menu
        {
            const auto targetPos = getPoint(widget, contextMenuEvent->pos(), parentWindow);
            auto fixedEvent = CE(contextMenuEvent->reason(), contextMenuEvent->pos(), targetPos);
            if (!qApp->sendEvent(watched, &fixedEvent) || !fixedEvent.isAccepted())
                return QObject::eventFilter(watched, event);

            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

}   // unnamed namespace

QPoint QnHiDpiWorkarounds::scaledToGlobal(const QPoint& scaled)
{
    return convertScaledToGlobal(scaled);
}

QAction* QnHiDpiWorkarounds::showMenu(QMenu* menu, const QPoint& globalPoint)
{
    if (!knownCorrectors.contains(menu))
        installMenuMouseEventCorrector(menu);

    const auto corrector = knownCorrectors.value(menu);
    corrector->setTargetPosition(globalPoint);

    const auto guard = QnRaiiGuard::create(
        []() { customMenuIsVisible = true; },
        []() { customMenuIsVisible = false; });
    return menu->exec(globalPoint);
}

void QnHiDpiWorkarounds::showMenuOnWidget(QWidget* widget, const QPoint& offset, QMenu* menu)
{
    showMenu(menu, getPoint(widget, offset));
}

QPoint QnHiDpiWorkarounds::safeMapToGlobal(QWidget* widget, const QPoint& offset)
{
    return getPoint(widget, offset);
}

void QnHiDpiWorkarounds::init()
{
    qApp->installEventFilter(new Test());
}

