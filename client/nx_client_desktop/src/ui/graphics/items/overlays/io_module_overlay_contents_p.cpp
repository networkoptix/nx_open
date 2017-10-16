#include "io_module_overlay_contents_p.h"

#include <nx/client/core/utils/geometry.h>
#include <ui/common/text_pixmap_cache.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <nx/utils/unused.h>

using nx::client::core::utils::Geometry;

namespace {

constexpr qreal kDirtyWidth = -1.0;

static inline bool isWidthDirty(qreal value)
{
    return value < 0.0;
}

static void paintPixmap(
    QPainter* painter,
    const QPixmap& pixmap,
    const QRectF& rect,
    const Qt::Alignment alignment)
{
    const auto pixmapSize = pixmap.size() / pixmap.devicePixelRatio();
    const auto pixmapRect = Geometry::aligned(pixmapSize, rect, alignment);
    paintPixmapSharp(painter, pixmap, pixmapRect.topLeft());
}

} // namespace

/*
QnIoModuleOverlayContentsPrivate::PortItem
*/

QnIoModuleOverlayContentsPrivate::PortItem::PortItem(
    const QString& id,
    bool isOutput,
    QGraphicsItem* parent)
    :
    GraphicsWidget(parent),
    m_id(id),
    m_isOutput(isOutput),
    m_on(false),
    m_minIdWidth(0.0),
    m_idWidth(kDirtyWidth),
    m_labelWidth(kDirtyWidth),
    m_elidedLabelWidth(kDirtyWidth)
{
    setTransparentForMouse(!isEnabled());
}

bool QnIoModuleOverlayContentsPrivate::PortItem::isOutput() const
{
    return m_isOutput;
}

QString QnIoModuleOverlayContentsPrivate::PortItem::id() const
{
    return m_id;
}

QString QnIoModuleOverlayContentsPrivate::PortItem::label() const
{
    return m_label;
}

void QnIoModuleOverlayContentsPrivate::PortItem::setLabel(const QString& label)
{
    if (m_label == label)
        return;

    m_label = label;
    m_elidedLabelWidth = kDirtyWidth;
    update();
}

bool QnIoModuleOverlayContentsPrivate::PortItem::isOn() const
{
    return m_on;
}

void QnIoModuleOverlayContentsPrivate::PortItem::setOn(bool on)
{
    if (m_on == on)
        return;

    m_on = on;
    update();
}

qreal QnIoModuleOverlayContentsPrivate::PortItem::minIdWidth() const
{
    return m_minIdWidth;
}

void QnIoModuleOverlayContentsPrivate::PortItem::setMinIdWidth(qreal value)
{
    if (qFuzzyIsNull(m_minIdWidth - value))
        return;

    m_minIdWidth = value;
    update();
}

qreal QnIoModuleOverlayContentsPrivate::PortItem::idWidth() const
{
    if (isWidthDirty(m_idWidth))
        m_idWidth = QFontMetricsF(m_activeIdFont).size(Qt::TextSingleLine, m_id).width();

    return m_idWidth;
}

qreal QnIoModuleOverlayContentsPrivate::PortItem::labelWidth() const
{
    if (isWidthDirty(m_labelWidth))
        m_labelWidth = QFontMetricsF(m_labelFont).size(Qt::TextSingleLine, m_label).width();

    return m_labelWidth;
}

qreal QnIoModuleOverlayContentsPrivate::PortItem::effectiveIdWidth() const
{
    return qMax(idWidth(), m_minIdWidth);
}

void QnIoModuleOverlayContentsPrivate::PortItem::setTransparentForMouse(bool transparent)
{
    setAcceptedMouseButtons(transparent ? Qt::NoButton : Qt::AllButtons);
    setAcceptHoverEvents(!transparent);
}

void QnIoModuleOverlayContentsPrivate::PortItem::ensureElidedLabel(qreal width) const
{
    bool elidedLabelDirty = !qFuzzyIsNull(m_elidedLabelWidth - width);
    if (!elidedLabelDirty)
        return;

    m_elidedLabel = QFontMetricsF(m_labelFont).elidedText(m_label, Qt::ElideRight, width);
    m_elidedLabelWidth = width;
}

void QnIoModuleOverlayContentsPrivate::PortItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
    QN_UNUSED(option, widget);
    paint(painter);
}

void QnIoModuleOverlayContentsPrivate::PortItem::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);
    switch (event->type())
    {
        case QEvent::FontChange:
            setupFonts(m_idFont, m_activeIdFont, m_labelFont);
            m_elidedLabelWidth = m_idWidth = m_labelWidth = kDirtyWidth;
            updateGeometry(); //< because m_minIdWidth is irrelevant now
            break;

        case QEvent::EnabledChange:
            setTransparentForMouse(!isEnabled());
            break;

        default:
            break;
    }
}

void QnIoModuleOverlayContentsPrivate::PortItem::paintId(
    QPainter* painter, const QRectF& rect, bool on)
{
    const auto pixmap = QnTextPixmapCache::instance()->pixmap(m_id,
        on ? m_activeIdFont : m_idFont,
        palette().color(on ? QPalette::HighlightedText : QPalette::WindowText));

    paintPixmap(painter, pixmap, rect, Qt::AlignLeft | Qt::AlignVCenter);
}

void QnIoModuleOverlayContentsPrivate::PortItem::paintLabel(
    QPainter* painter, const QRectF& rect, Qt::AlignmentFlag horizontalAlignment)
{
    ensureElidedLabel(rect.width());

    const auto pixmap = QnTextPixmapCache::instance()->pixmap(m_elidedLabel,
        m_labelFont, palette().color(QPalette::ButtonText));

    paintPixmap(painter, pixmap, rect, horizontalAlignment | Qt::AlignVCenter);
}

/*
QnIoModuleOverlayContentsPrivate::InputPortItem
*/

QnIoModuleOverlayContentsPrivate::InputPortItem::InputPortItem(
    const QString& id,
    QGraphicsItem* parent)
    :
    PortItem(id, false/*isOutput*/, parent)
{
}

/*
QnIoModuleOverlayContentsPrivate::OutputPortItem
*/

QnIoModuleOverlayContentsPrivate::OutputPortItem::OutputPortItem(
    const QString& id,
    ClickHandler clickHandler,
    QGraphicsItem* parent)
    :
    base_type(id, true/*isOutput*/, parent),
    m_clickHandler(clickHandler),
    m_hovered(false)
{
    setClickableButtons(Qt::LeftButton);
}

bool QnIoModuleOverlayContentsPrivate::OutputPortItem::isHovered() const
{
    return isEnabled() && m_hovered;
}

QPainterPath QnIoModuleOverlayContentsPrivate::OutputPortItem::shape() const
{
    QPainterPath result;
    result.addRect(activeRect());
    return result;
}

bool QnIoModuleOverlayContentsPrivate::OutputPortItem::sceneEvent(QEvent* event)
{
    switch (event->type())
    {
        /* Update hover in pressed state: */
        case QEvent::GraphicsSceneMouseMove:
            m_hovered = activeRect().contains(
                static_cast<QGraphicsSceneMouseEvent*>(event)->pos());
            break;

        /* Update hover in released state: */
        case QEvent::GraphicsSceneHoverEnter:
        case QEvent::GraphicsSceneHoverMove:
            m_hovered = true;
            break;
        case QEvent::GraphicsSceneHoverLeave:
            m_hovered = false;
            break;

        default:
            break;
    }

    return base_type::sceneEvent(event);
}

void QnIoModuleOverlayContentsPrivate::OutputPortItem::clickedNotify(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event);

    if (!isEnabled() || !m_clickHandler)
        return;

    m_clickHandler(id());
}

/*
QnIoModuleOverlayContentsPrivate::Layout
*/

QnIoModuleOverlayContentsPrivate::Layout::Layout(QGraphicsLayoutItem* parent) :
    QGraphicsLayout(parent),
    m_layoutDirty(false)
{
}

/* Clear layout and destroy all items: */
void QnIoModuleOverlayContentsPrivate::Layout::clear()
{
    for (auto item: m_itemsById)
        item->deleteLater();

    m_itemsById.clear();
    m_inputItems.clear();
    m_outputItems.clear();
    invalidate();
}

/* Add new input or output item sequentially: */
bool QnIoModuleOverlayContentsPrivate::Layout::addItem(PortItem* item)
{
    if (!item || itemById(item->id()))
        return false;

    auto& list = item->isOutput() ? m_outputItems : m_inputItems;
    list << ItemWithPosition({ item, Position() });

    m_itemsById[item->id()] = item;

    addChildLayoutItem(item);
    invalidate();

    return true;
}

/* Item by string id: */
QnIoModuleOverlayContentsPrivate::PortItem*
    QnIoModuleOverlayContentsPrivate::Layout::itemById(const QString& id)
{
    auto it = m_itemsById.find(id);
    if (it != m_itemsById.end())
        return *it;

    return nullptr;
}

/* Total item count in the layout: a method required by layout engine: */
int QnIoModuleOverlayContentsPrivate::Layout::count() const
{
    auto count = m_inputItems.size() + m_outputItems.size();
    NX_EXPECT(count == m_itemsById.size());
    return count;
}

/* A helper method to find item location by linear index: */
QnIoModuleOverlayContentsPrivate::Layout::Location
    QnIoModuleOverlayContentsPrivate::Layout::locate(int index)
{
    NX_ASSERT(index >= 0 || index < count());

    if (index < m_inputItems.size())
        return Location(m_inputItems, index);

    return Location(m_outputItems, index - m_inputItems.size());
}

/* A helper method to find item location by linear index: */
QnIoModuleOverlayContentsPrivate::Layout::ConstLocation
    QnIoModuleOverlayContentsPrivate::Layout::locate(int index) const
{
    NX_ASSERT(index >= 0 || index < count());

    if (index < m_inputItems.size())
        return ConstLocation(m_inputItems, index);

    return ConstLocation(m_outputItems, index - m_inputItems.size());
}

/* Item by linear index: a method required by layout engine: */
QnIoModuleOverlayContentsPrivate::Layout::ItemWithPosition&
    QnIoModuleOverlayContentsPrivate::Layout::itemWithPosition(int index)
{
    auto location = locate(index);
    return location.first[location.second];
}

/* Item by linear index: a method required by layout engine: */
QGraphicsLayoutItem* QnIoModuleOverlayContentsPrivate::Layout::itemAt(int index) const
{
    auto location = locate(index);
    return location.first[location.second].item;
}

/* Remove item by linear index: a method required by layout engine: */
void QnIoModuleOverlayContentsPrivate::Layout::removeAt(int index)
{
    auto location = locate(index);
    auto item = location.first[location.second].item;
    location.first.removeAt(location.second);
    m_itemsById.remove(item->id());
    invalidate();
}

/* Invalidate layout contents: */
void QnIoModuleOverlayContentsPrivate::Layout::invalidate()
{
    base_type::invalidate();
    m_layoutDirty = true;
}

/* Recalculate item geometries: */
void QnIoModuleOverlayContentsPrivate::Layout::setGeometry(const QRectF& rect)
{
    base_type::setGeometry(rect);
    ensureLayout();
}

/* Lay items by columns and rows: */
void QnIoModuleOverlayContentsPrivate::Layout::ensureLayout()
{
    if (!m_layoutDirty)
        return;

    recalculateLayout();
    m_layoutDirty = false;
}

/*
QnIoModuleOverlayContentsPrivate
*/

QnIoModuleOverlayContentsPrivate::QnIoModuleOverlayContentsPrivate(
    QnIoModuleOverlayContents* main, Layout* layout)
    :
    base_type(main),
    q_ptr(main),
    m_layout(layout)
{
    main->setLayout(m_layout);
}

QnIoModuleOverlayContentsPrivate::PortItem*
    QnIoModuleOverlayContentsPrivate::createItem(const QnIOPortData& port)
{
    Q_Q(QnIoModuleOverlayContents);
    if (!q->overlayWidget())
        return nullptr;

    bool isOutput = port.portType == Qn::PT_Output;
    if (!isOutput && port.portType != Qn::PT_Input)
        return nullptr;

    /* Create new item: */
    auto item = createItem(port.id, isOutput);
    if (!item)
        return nullptr;

    item->setLabel(port.getName());
    m_layout->addItem(item);
    return item;
}

void QnIoModuleOverlayContentsPrivate::portsChanged(
    const QnIOPortDataList& ports,
    bool userInputEnabled)
{
    m_layout->clear();

    for (const auto& port: ports)
    {
        if (auto item = createItem(port))
            item->setEnabled(userInputEnabled && item->isOutput());
    }
}

void QnIoModuleOverlayContentsPrivate::stateChanged(
    const QnIOPortData& port,
    const QnIOStateData& state)
{
    Q_UNUSED(port); //< unused for now
    if (auto item = m_layout->itemById(state.id))
        item->setOn(state.isActive);
}

QnIoModuleOverlayContentsPrivate::Layout* QnIoModuleOverlayContentsPrivate::layout() const
{
    return m_layout;
}
