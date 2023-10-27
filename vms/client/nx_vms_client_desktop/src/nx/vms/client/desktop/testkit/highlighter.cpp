// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "highlighter.h"

#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsObject>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>

#include <client_core/client_core_module.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/testkit/utils.h>

using namespace std::chrono;

namespace {

static constexpr auto kPickInterval = 300ms;
static constexpr auto kHideTimeout = 2s;

std::pair<QObject*, QScreen*> getTopLevelAt(QPoint globalPos, QWidget* skip)
{
    const auto widgets = QApplication::topLevelWidgets();

    int minValue = std::numeric_limits<int>::max();
    QWidget* minWidget = nullptr;

    for (auto& widget: widgets)
    {
        if (widget == skip || !widget->isVisible() || !widget->window()->isActiveWindow())
            continue;

        const auto rect = nx::vms::client::desktop::testkit::utils::globalRect(
            QVariant::fromValue(widget));

        if (!rect.contains(globalPos))
            continue;

        const int newMinValue = rect.width() + rect.height();
        if (newMinValue < minValue)
        {
            minValue = newMinValue;
            minWidget = widget;
        }
    }

    if (minWidget)
        return {minWidget, minWidget->screen()};

    minValue = std::numeric_limits<int>::max();
    QWindow* minWindow = nullptr;

    const auto windows = QGuiApplication::topLevelWindows();
    for (auto& window: windows)
    {
        if (!window->isVisible() || !window->isActive())
            continue;

        const auto rect = window->geometry();
        if (rect.contains(globalPos))
        {
            const int newMinValue = rect.size().width() + rect.size().height();
            if (newMinValue < minValue)
                minWindow = window;
        }
    }

    if (minWindow)
        return {minWindow, minWindow->screen()};

    return {nullptr, nullptr};
}

} // namespace

namespace nx::vms::client::desktop::testkit {

class FullScreenOverlay: public QQuickWidget
{
    using base_type = QQuickWidget;

public:
    FullScreenOverlay(QQmlEngine* engine, QWidget* parent = nullptr): base_type(engine, parent)
    {
        const Qt::WindowFlags kExtraFlags(
            Qt::ToolTip
            | Qt::FramelessWindowHint
            | Qt::NoDropShadowWindowHint
            | Qt::WindowTransparentForInput
            | Qt::WindowDoesNotAcceptFocus
            | Qt::BypassGraphicsProxyWidget);

        setWindowFlags(kExtraFlags);
        setAttribute(Qt::WA_TranslucentBackground);
        setClearColor(Qt::transparent);
        setStyleSheet("background:transparent;");
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);

        // Workaround for some drivers drawing transparent OpenGL windows as opaque.
        setWindowOpacity(ini().elementHighlighterOverlayOpacity);
    }
};

Highlighter::Highlighter(QObject* parent): base_type(parent)
{
    qApp->installEventFilter(this);
    m_pickTimer.setInterval(kPickInterval);
    m_pickTimer.setSingleShot(false);

    m_hideTimer.setInterval(kHideTimeout);
    m_hideTimer.setSingleShot(true);

    connect(&m_pickTimer, &QTimer::timeout, this,
        [this]
        {
            if (!qGuiApp->focusWindow())
                return;
            const auto pos = QCursor::pos();
            highlight(pick(pos), kHideTimeout);
        });

    connect(&m_hideTimer, &QTimer::timeout, this, [this]{ m_overlay->hide(); });
}

Highlighter::~Highlighter()
{
    qApp->removeEventFilter(this);
}

void Highlighter::setEnabled(bool enabled)
{
    if (enabled)
    {
        m_pickTimer.start();
        return;
    }

    m_pickTimer.stop();
    if (m_overlay)
        m_overlay->hide();
}

bool Highlighter::isEnabled() const
{
    return m_pickTimer.isActive();
}

QVariant Highlighter::pick(QPoint globalPos) const
{
    auto [topLevel, screen] = getTopLevelAt(globalPos, m_overlay.get());
    if (!topLevel)
        return {};

    QSet<QObject*> visited;

    int minValue = std::numeric_limits<int>::max();
    QPointer<QObject> minObject = nullptr;
    QRect minRect;

    auto selectContaining =
        [&](utils::VisitObject visitObject) -> bool
        {
            if (!std::holds_alternative<QObject*>(visitObject))
                return false;

            auto object = std::get<QObject*>(visitObject);

            if (visited.contains(object))
                return false;
            visited.insert(object);

            const auto globalRect = utils::globalRect(QVariant::fromValue(object));

            if (globalRect.contains(globalPos) && globalRect.width() > 0 && globalRect.height() > 0)
            {
                // QQuickWindow may be offscreen and thus invisible.
                if (object->property("visible").toBool() || qobject_cast<QQuickWindow*>(object))
                {
                    const int newMinValue = globalRect.width() + globalRect.height();
                    if (newMinValue < minValue)
                    {
                        minValue = newMinValue;
                        minObject = object;
                        minRect = globalRect;
                    }
                    return true;
                }
            }

            if (auto item = qobject_cast<QQuickItem*>(object); item && !item->clip())
                return true;
            else if (auto item = qobject_cast<QGraphicsObject*>(object); item && !item->isClipped())
                return true;

            return false;
        };

    utils::visitTree(topLevel, selectContaining);

    if (!minObject)
        return {};

    QSize size;
    // For debug purposes show the size reported by the item itself.
    if (auto item = qobject_cast<QQuickItem*>(minObject))
        size = item->size().toSize();
    else if (auto graphicsObject = qobject_cast<QGraphicsObject*>(minObject))
        size = graphicsObject->boundingRect().size().toSize();
    else
        size = minRect.size();

    QString className = minObject->metaObject()->className();
    auto [name, location] = utils::nameAndBaseUrl(minObject);

    const QRect screenGeometry = screen->geometry();
    const QRect rect(minRect.topLeft() - screenGeometry.topLeft(), minRect.size());

    QVariantMap data;
    data.insert("name", name);
    data.insert("className", className);
    data.insert("location", location);
    data.insert("properties", utils::dumpQObject(minObject));
    data.insert("rect", rect);
    data.insert("size", size);
    data.insert("screen", screenGeometry);

    return data;
}

void Highlighter::highlight(QVariant data, std::chrono::milliseconds hideTimeout)
{
    if (data.isNull())
        return;

    if (!m_overlay)
    {
        m_overlay = std::make_unique<FullScreenOverlay>(appContext()->qmlEngine());
        m_overlay->setSource(QUrl("Nx/Debug/HighlighterOverlay.qml"));
        m_overlay->setResizeMode(QQuickWidget::SizeRootObjectToView);
    }

    m_hideTimer.setInterval(hideTimeout);
    m_hideTimer.start();

    m_overlay->setGeometry(data.toMap()["screen"].toRect());

    QVariantList highlights;
    highlights << data;
    m_overlay->rootObject()->setProperty("highlights", highlights);
    m_overlay->show();
    m_overlay->raise();
}

} // namespace nx::vms::client::desktop::testkit
