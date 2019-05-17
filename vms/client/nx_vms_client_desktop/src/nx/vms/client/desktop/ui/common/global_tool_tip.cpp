#include "global_tool_tip.h"

#include <QtCore/QTimer>
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

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

public slots:
    void initEventFilter();
    void handleToolTipAboutToShow();
    void handleToolTipParentChanged();
    void handleToolTipClosed();

public:
    GlobalToolTipAttached* q = nullptr;
    QTimer* showTimer = nullptr;
    QQuickItem* item = nullptr;
    QString text;
    QPoint hoverPos;
    bool visible = false;
    bool showOnHover = true;
    int delayMs = 500;
    int shortDelayMs = 100;
    bool hoverEnabledOrigignal = false;
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
            hoverEnabledOrigignal = hoverEnabled.toBool();
            item->setProperty("hoverEnabled", true);
        }
        else
        {
            hoverEnabledOrigignal = item->acceptHoverEvents();
            item->setAcceptHoverEvents(true);
        }

        item->installEventFilter(this);
    }
    else
    {
        item->removeEventFilter(this);

        if (const QVariant& hoverEnabled = item->property("hoverEnabled"); hoverEnabled.isValid())
            item->setProperty("hoverEnabled", hoverEnabledOrigignal);
        else
            item->setAcceptHoverEvents(hoverEnabledOrigignal);
    }
}

void GlobalToolTipAttached::Private::adjustPosition()
{
    auto toolTip = instance();

    if (!item || !toolTip)
        return;

    QQuickItem* root = item->window()->contentItem();
    if (!root)
        return;

    const QSize rootSize = item->window()->size();

    constexpr qreal cursorPadding = 16;
    constexpr qreal topEdgePadding = 2;

    const qreal w = toolTip.width();
    const qreal h = toolTip.height();

    const QRectF itemRect = item->mapRectToItem(root, QRectF(QPointF(), item->size()));
    const QPointF hoverPos = item->mapToItem(root, this->hoverPos);

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

    const QPointF pos = item->mapFromItem(root, QPointF(x, y));
    toolTip.setPosition(pos);
}

void GlobalToolTipAttached::Private::showImmediately()
{
    auto toolTip = instance(true);
    toolTip.setParentItem(item);
    update();

    connect(toolTip, SIGNAL(aboutToShow()), this, SLOT(handleToolTipAboutToShow()));
    connect(toolTip, SIGNAL(parentChanged()), this, SLOT(handleToolTipParentChanged()));
    connect(toolTip, SIGNAL(closed()), this, SLOT(handleToolTipClosed()));

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
        toolTip.setText(text);
        adjustPosition();
    }
}

bool GlobalToolTipAttached::Private::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != item)
        return false;

    switch (event->type())
    {
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::HoverMove:
        {
            const auto hover = static_cast<QHoverEvent*>(event);
            hoverPos = hover->pos();

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

    if (toolTip.parentItem() != item)
        return;

    if (!toolTip.isVisible())
        adjustPosition();
}

void GlobalToolTipAttached::Private::handleToolTipParentChanged()
{
    auto toolTip = instance();

    if (toolTip && toolTip.parentItem() != item)
    {
        disconnect(toolTip, SIGNAL(aboutToShow()), this, SLOT(handleToolTipAboutToShow()));
        disconnect(toolTip, SIGNAL(parentChanged()), this, SLOT(handleToolTipParentChanged()));
        disconnect(toolTip, SIGNAL(closed()), this, SLOT(handleToolTipClosed()));
    }
}

void GlobalToolTipAttached::Private::handleToolTipClosed()
{
    if (auto toolTip = instance())
        toolTip.setParentItem(nullptr);
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

void GlobalToolTipAttached::setText(const QString& text)
{
    if (d->text == text)
        return;

    d->text = text;
    emit textChanged();

    if (auto toolTip = d->instance(); toolTip && toolTip.isVisible())
        d->update();
}

bool GlobalToolTipAttached::isVisible() const
{
    return d->visible;
}

void GlobalToolTipAttached::setVisible(bool visible)
{
    if (d->visible == visible)
        return;

    d->visible = visible;
    emit visibleChanged();
}

bool GlobalToolTipAttached::showOnHover() const
{
    return d->showOnHover;
}

void GlobalToolTipAttached::setShowOnHover(bool showOnHover)
{
    if (d->showOnHover == showOnHover)
        return;

    d->showOnHover = showOnHover;
    d->setShowOnHoverInternal(showOnHover);
    emit showOnHoverChanged();
}

int GlobalToolTipAttached::delay() const
{
    return d->delayMs;
}

void GlobalToolTipAttached::setDelay(int delay)
{
    if (d->delayMs == delay)
        return;

    d->delayMs = delay;
    emit delayChanged();
}

void GlobalToolTipAttached::show(const QString& text, int delay)
{
    if (!d->item || d->text.isEmpty())
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
    else if (d->item != toolTip.parentItem())
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
    if (toolTip.parentItem() != d->item)
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
