
#include "hidpi_workarounds.h"

#include <utils/common/connective.h>

namespace {

typedef QMultiMap<int, QScreen*> SortedScreens;
QPoint convertScaledToGlobal(const QPoint& scaled)
{

    QPoint result(scaled);

    SortedScreens xSorted;
    for (auto screen : QGuiApplication::screens())
        xSorted.insert(screen->geometry().left(), screen);

    bool found = false;
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

QPoint screenRelatedToGlobal(const QPoint& point, QScreen* screen)
{
    const auto geometry = screen->geometry();
    const auto topLeft = geometry.topLeft();
    const auto left = topLeft.x();

    const auto scaledPointOnScreen = (point - topLeft);
    if (scaledPointOnScreen.x() > 0 && scaledPointOnScreen.y() > 0)
        return point;

    const auto pixelPointOnScreen = scaledPointOnScreen * screen->devicePixelRatio();
    SortedScreens tmp;
    for (auto src : QGuiApplication::screens())
        tmp.insert(src->geometry().left(), src);


    // Searching for prev screen
    const auto xSorted = tmp.values().toVector();
    for (auto it = xSorted.rbegin(); it != xSorted.rend(); ++it)
    {
        const auto src = *it;
        if (src->geometry().left() >= left)
            continue;

        // We've found prev screen
        const auto factor = src->devicePixelRatio();
        const auto targetGeometry = src->geometry();
        const auto targetTopLeft = targetGeometry.topLeft();
        return targetTopLeft + QPoint(targetGeometry.width(), 0)
            + pixelPointOnScreen / factor;
    }
    return QPoint();
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
        {
            window->setScreen(m_screen);
            qDebug() << "--target pos : " << m_screen;
        }
    }

    virtual bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!m_screen || (event->type() != QEvent::Show))
            return QObject::eventFilter(watched, event);

        const auto window = getWindow();
        if (!window)
            return QObject::eventFilter(watched, event);

        window->setScreen(m_screen);
        qDebug() << "--show: " << m_screen;
        connect(window, &QWindow::screenChanged, this,
            [window, this](QScreen* newScreen)
            {
                if (window && (m_screen != newScreen))
                {
                    window->setScreen(m_screen);
                    qDebug() << "--changed: " << m_screen << newScreen;
                }
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

static QWindow* viewportWindow = nullptr;
void setMouseWorkaroundActive(bool active, QWindow* window)
{
    if (!window)
        return;

    const auto flags = (window->flags() & ~Qt::WindowType_Mask);
    window->setFlags(flags | (active ? Qt::ForeignWindow : Qt::Widget));
}

}   // unnamed namespace

QPoint QnHiDpiWorkarounds::scaledToGlobal(const QPoint& scaled)
{
    return convertScaledToGlobal(scaled);
}

QAction* QnHiDpiWorkarounds::showMenu(QMenu* menu, const QPoint& globalPoint)
{
    if (!viewportWindow)
        return nullptr;

    if (!knownCorrectors.contains(menu))
        installMenuMouseEventCorrector(menu);

    const auto corrector = knownCorrectors.value(menu);
    const auto pos = QCursor::pos();
    qDebug() << pos << globalPoint;
    corrector->setTargetPosition(globalPoint);
    return menu->exec(globalPoint);
}

void QnHiDpiWorkarounds::toolButtonMenuWorkaround(QToolButton* button, QMenu* menu)
{
    if (!viewportWindow)
        return;

    // Checks if we has placed title bar to proxy widget
    const bool isGraphicsEnvironment = (button->parent() && !button->parent()->parent());
    const auto menuLocalPos = button->rect().bottomLeft();

    setMouseWorkaroundActive(false, viewportWindow);
    const auto globalPos = button->mapToGlobal(menuLocalPos);
    setMouseWorkaroundActive(true, viewportWindow);

    const auto fixedGlobalPoint = screenRelatedToGlobal(globalPos, viewportWindow->screen());

    showMenu(menu, fixedGlobalPoint);
}

void QnHiDpiWorkarounds::setViewportWindow(QWindow* window)
{
    if (viewportWindow)
        setMouseWorkaroundActive(false, viewportWindow);

    viewportWindow = window;
    if (viewportWindow)
        setMouseWorkaroundActive(true, viewportWindow);
}
