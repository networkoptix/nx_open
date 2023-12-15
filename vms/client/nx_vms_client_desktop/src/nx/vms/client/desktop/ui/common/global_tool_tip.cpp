// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "global_tool_tip.h"

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QTimer>

#include <QtQml/QtQml>

#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickItem>

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>

namespace nx::vms::client::desktop {

namespace {

struct ToolTipHelper
{
    QObject* object = nullptr;

    QObject* operator->() { return object; }
    operator bool() const { return object; }
    operator QObject*() const { return object; }

    QVariant property(const char* name) const
    {
        return object->property(name);
    }
    void setProperty(const char* name, const QVariant& value)
    {
        object->setProperty(name, value);
    }

    QQuickItem* parentItem() const
    {
        return qobject_cast<QQuickItem*>(property("parent").value<QObject*>());
    }
    void setParentItem(QQuickItem* parent)
    {
        setProperty("parent", QVariant::fromValue(parent));
    }

    QQuickItem* invokerItem() const
    {
        return qobject_cast<QQuickItem*>(property("invoker").value<QObject*>());
    }
    void setInvokerItem(QQuickItem* invoker)
    {
        setProperty("invoker", QVariant::fromValue(invoker));
    }

    bool isVisible() const
    {
        return property("visible").toBool();
    }

    void setPosition(const QPointF& position)
    {
        setProperty("x", position.x());
        setProperty("y", position.y());
    }

    qreal width() const
    {
        return property("width").toDouble();
    }
    qreal height() const
    {
        return property("height").toDouble();
    }

    QString text() const
    {
        return property("text").toString();
    }
    void setText(const QString& text)
    {
        setProperty("text", text);
    }

    void open()
    {
        QMetaObject::invokeMethod(object, "open");
    }
    void close()
    {
        QMetaObject::invokeMethod(object, "close");
    }
};

static const char* kTargetItemProperty = "_nx_globalToolTipTargetItem";

} // namespace

class GlobalToolTipAttached::Private: public QObject
{
    Q_OBJECT

public:
    Private(GlobalToolTipAttached* q, QObject* parent);
    virtual ~Private() override;

    ToolTipHelper instance(bool create = false);
    void setShowOnHoverInternal(bool showOnHover);
    void adjustPosition();
    void showImmediately();
    void showDelayed(int delay, bool restart);
    void update();

    bool isEffectivelyDisabled() const { return !enabled || text.isEmpty(); }

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

public slots:
    void initEventFilter();
    void handleToolTipAboutToShow();
    void handleToolTipInvokerChanged();
    void handleToolTipClosed();

public:
    GlobalToolTipAttached* q = nullptr;
    QTimer* showTimer = nullptr;
    QQuickItem* item = nullptr;
    QString text;
    QPoint hoverPos;
    bool enabled = true;
    bool showOnHover = true;
    bool hovered = false;
    bool stickToItem = false;
    int delayMs = 500;
    int shortDelayMs = 100;
    bool hoverEnabledOriginal = false;
};

GlobalToolTipAttached::Private::Private(GlobalToolTipAttached* q, QObject* parent):
    q(q),
    showTimer(new QTimer(this)),
    item(qobject_cast<QQuickItem*>(parent))
{
    if (QStyle* style = qApp->style())
        delayMs = style->styleHint(QStyle::SH_ToolTip_WakeUpDelay);

    if (item && showOnHover)
    {
        QObject* attached = qmlAttachedPropertiesObject<QQmlComponent>(item);
        connect(attached, SIGNAL(completed()), this, SLOT(initEventFilter()));
    }

    showTimer->setSingleShot(true);
    connect(showTimer, &QTimer::timeout, this,
        [this]()
        {
            QQmlEngine* engine = qmlEngine(item);
            if (!engine)
                return;

            if (engine->property(kTargetItemProperty).value<QQuickItem*>() == item)
                showImmediately();
        });
}

GlobalToolTipAttached::Private::~Private()
{
    if (item)
        item->removeEventFilter(this);
}

ToolTipHelper GlobalToolTipAttached::Private::instance(bool create)
{
    if (!item)
        return {};

    QQmlEngine* engine = qmlEngine(item);
    if (!engine)
        return {};

    static const char* kInstanceProperty = "_nx_globalToolTip";

    ToolTipHelper toolTip{engine->property(kInstanceProperty).value<QObject*>()};
    if (!toolTip && create)
    {
        QQmlComponent component(engine);
        component.setData("import Nx.Controls 1.0; ToolTip { delay: 0 }", QUrl());

        toolTip.object = component.create();
        if (!toolTip)
            return {};

        engine->setProperty(kInstanceProperty, QVariant::fromValue(toolTip.object));
    }

    return ToolTipHelper{toolTip};
}

void GlobalToolTipAttached::Private::setShowOnHoverInternal(bool showOnHover)
{
    if (!item)
        return;

    if (showOnHover)
    {
        if (const QVariant& hoverEnabled = item->property("hoverEnabled"); hoverEnabled.isValid())
        {
            hoverEnabledOriginal = hoverEnabled.toBool();
            item->setProperty("hoverEnabled", true);
        }
        else
        {
            hoverEnabledOriginal = item->acceptHoverEvents();
            item->setAcceptHoverEvents(true);
        }

        item->installEventFilter(this);
    }
    else
    {
        item->removeEventFilter(this);

        if (const QVariant& hoverEnabled = item->property("hoverEnabled"); hoverEnabled.isValid())
            item->setProperty("hoverEnabled", hoverEnabledOriginal);
        else
            item->setAcceptHoverEvents(hoverEnabledOriginal);
    }
}

void GlobalToolTipAttached::Private::adjustPosition()
{
    auto toolTip = instance();

    if (!item || !toolTip || !item->window())
        return;

    QQuickItem* parent = toolTip.parentItem();
    if (!parent)
        return;

    const QSize rootSize = item->window()->size();

    constexpr qreal cursorPadding = 16;
    constexpr qreal topEdgePadding = 2;

    const qreal w = toolTip.width();
    const qreal h = toolTip.height();

    const QRectF itemRect = item->mapRectToItem(parent, QRectF(QPointF(), item->size()));
    const QPointF hoverPos = item->mapToItem(parent, this->hoverPos);

    qreal x = qBound(0.0, hoverPos.x(), rootSize.width() - w);

    constexpr qreal maxToolTipDistance = 32;

    qreal y;

    if (hoverPos.y() + cursorPadding + h < rootSize.height())
    {
        y = hoverPos.y() + cursorPadding;
    }
    else if (itemRect.top() - topEdgePadding - h >= 0
        && hoverPos.y() - (itemRect.top() - topEdgePadding) <= maxToolTipDistance)
    {
        y = itemRect.top() - topEdgePadding - h;
    }
    else
    {
        y = qBound(0.0, hoverPos.y() + cursorPadding, rootSize.height() - h);
    }

    toolTip.setPosition(QPointF(x, y));
}

void GlobalToolTipAttached::Private::showImmediately()
{
    auto toolTip = instance(true);
    if (toolTip.invokerItem() != item)
    {
        toolTip.setInvokerItem(item);
        connect(toolTip, SIGNAL(aboutToShow()), this, SLOT(handleToolTipAboutToShow()));
        connect(toolTip, SIGNAL(invokerChanged()), this, SLOT(handleToolTipInvokerChanged()));
        connect(toolTip, SIGNAL(closed()), this, SLOT(handleToolTipClosed()));
    }

    update();
    toolTip.open();
}

void GlobalToolTipAttached::Private::showDelayed(int delay, bool restart = true)
{
    if (restart || !showTimer->isActive())
        showTimer->start(delay);
}

void GlobalToolTipAttached::Private::update()
{
    if (auto toolTip = instance())
    {
        if (stickToItem)
        {
            toolTip.setParentItem(toolTip.invokerItem());
        }
        else
        {
            const auto invoker = toolTip.invokerItem();
            toolTip.setParentItem(invoker && invoker->window()
                ? invoker->window()->contentItem()
                : nullptr);
        }

        toolTip.setText(text);
        adjustPosition();
    }
}

bool GlobalToolTipAttached::Private::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != item || isEffectivelyDisabled())
        return false;

    switch (event->type())
    {
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::HoverMove:
        {
            const auto hover = static_cast<QHoverEvent*>(event);
            hoverPos = hover->pos();
            hovered = event->type() != QEvent::HoverLeave;

            // Avoid restarting the tooltip show timer when there is no movement.
            if (event->type() == QEvent::HoverMove && hoverPos == hover->oldPos())
                break;

            if (event->type() == QEvent::HoverLeave)
                q->hide();
            else
                q->show(text);

            break;
        }
        case QEvent::KeyPress:
        case QEvent::MouseButtonPress:
            q->hide();
            break;
        default:
            break;
    }

    return false;
}

void GlobalToolTipAttached::Private::initEventFilter()
{
    setShowOnHoverInternal(showOnHover);
}

void GlobalToolTipAttached::Private::handleToolTipAboutToShow()
{
    auto toolTip = instance();

    if (toolTip.invokerItem() != item)
        return;

    if (!toolTip.isVisible())
        adjustPosition();
}

void GlobalToolTipAttached::Private::handleToolTipInvokerChanged()
{
    auto toolTip = instance();

    if (toolTip && toolTip.invokerItem() != item)
    {
        disconnect(toolTip, SIGNAL(aboutToShow()), this, SLOT(handleToolTipAboutToShow()));
        disconnect(toolTip, SIGNAL(invokerChanged()), this, SLOT(handleToolTipInvokerChanged()));
        disconnect(toolTip, SIGNAL(closed()), this, SLOT(handleToolTipClosed()));
    }
}

void GlobalToolTipAttached::Private::handleToolTipClosed()
{
    if (auto toolTip = instance())
    {
        toolTip.setInvokerItem(nullptr);
        toolTip.setParentItem(nullptr);
    }
}

//-------------------------------------------------------------------------------------------------

GlobalToolTipAttached::GlobalToolTipAttached(QObject* parent):
    QObject(parent),
    d(new Private(this, parent))
{
}

GlobalToolTipAttached::~GlobalToolTipAttached()
{
}

QString GlobalToolTipAttached::text() const
{
    return d->text;
}

void GlobalToolTipAttached::setText(const QString& value)
{
    if (d->text == value)
        return;

    d->text = value;
    emit textChanged();

    if (d->text.isEmpty())
    {
        hide();
    }
    else if (auto toolTip = d->instance();
        toolTip && toolTip.isVisible() && toolTip.invokerItem() == d->item)
    {
        d->update();
    }
}

bool GlobalToolTipAttached::isEnabled() const
{
    return d->enabled;
}

void GlobalToolTipAttached::setEnabled(bool value)
{
    if (d->enabled == value)
        return;

    d->enabled = value;
    emit enabledChanged();

    if (!d->enabled)
        hide();
    else if (d->hovered && d->showOnHover)
        show(d->text);
}

bool GlobalToolTipAttached::showOnHover() const
{
    return d->showOnHover;
}

void GlobalToolTipAttached::setShowOnHover(bool value)
{
    if (d->showOnHover == value)
        return;

    d->showOnHover = value;
    d->setShowOnHoverInternal(value);
    emit showOnHoverChanged();
}

bool GlobalToolTipAttached::stickToItem() const
{
    return d->stickToItem;
}

void GlobalToolTipAttached::setStickToItem(bool value)
{
    if (d->stickToItem == value)
        return;

    d->stickToItem = value;
    emit stickToItemChanged();

    if (auto toolTip = d->instance();
        toolTip && toolTip.isVisible() && toolTip.invokerItem() == d->item)
    {
        d->update();
    }
}

int GlobalToolTipAttached::delay() const
{
    return d->delayMs;
}

void GlobalToolTipAttached::setDelay(int value)
{
    if (d->delayMs == value)
        return;

    d->delayMs = value;
    emit delayChanged();
}

void GlobalToolTipAttached::show(const QString& text, int delay)
{
    if (!d->item || d->text.isEmpty() || d->isEffectivelyDisabled())
        return;

    QQmlEngine* engine = qmlEngine(d->item);
    if (!engine)
        return;

    auto toolTip = d->instance(true);
    if (!toolTip)
        return;

    if (delay >= 0)
        setDelay(delay);

    setText(text);

    engine->setProperty(kTargetItemProperty, QVariant::fromValue(d->item));

    if (!toolTip.isVisible())
        d->showDelayed(d->delayMs);
    else if (d->item != toolTip.invokerItem())
        d->showDelayed(d->shortDelayMs, false);
    else if (text != toolTip.text())
        d->showImmediately();
    else
        toolTip.open();
}

void GlobalToolTipAttached::hide()
{
    d->showTimer->stop();

    if (!d->item)
        return;

    auto toolTip = d->instance();
    if (!toolTip)
        return;

    // Items should only close ToolTip shown by itself.
    if (toolTip.invokerItem() != d->item)
        return;

    toolTip.close();
}

GlobalToolTip::GlobalToolTip(QObject* parent):
    QObject(parent)
{
}

GlobalToolTipAttached* GlobalToolTip::qmlAttachedProperties(QObject* parent)
{
    return new GlobalToolTipAttached(parent);
}

void GlobalToolTip::registerQmlType()
{
    qmlRegisterType<GlobalToolTip>("nx.vms.client.desktop", 1, 0, "GlobalToolTip");
}

} // namespace nx::vms::client::desktop

#include "global_tool_tip.moc"
