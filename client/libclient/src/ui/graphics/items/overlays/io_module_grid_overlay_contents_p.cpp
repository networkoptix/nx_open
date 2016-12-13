#include "io_module_grid_overlay_contents_p.h"

#include <api/model/api_ioport_data.h>

#include <ui/common/geometry.h>
#include <ui/common/text_pixmap_cache.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workaround/sharp_pixmap_painting.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/unused.h>

namespace {

/* Private file scope constants: */

constexpr int kIdFontPixelSize = 12;
constexpr int kIdFontWeight = QFont::Normal;
constexpr int kActiveIdFontWeight = QFont::Bold;
constexpr int kLabelFontPixelSize = 13;
constexpr int kLabelFontWeight = QFont::DemiBold;

constexpr int kIndicatorWidth = 28;

constexpr int kMinimumItemWidth = 160;
static const int kMinimumItemHeight = style::Metrics::kButtonHeight;

constexpr qreal kDirtyWidth = -1.0;

/* Private file scope functions: */

static inline bool isOdd(int value)
{
    return (value & 1) != 0;
}

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
    const auto pixmapRect = QnGeometry::aligned(pixmapSize, rect, alignment);
    paintPixmapSharp(painter, pixmap, pixmapRect.topLeft());
}

} // namespace

/*
QnIoModuleGridOverlayContentsPrivate::PortItem
*/

QnIoModuleGridOverlayContentsPrivate::PortItem::PortItem(
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
    m_elidedLabelWidth(kDirtyWidth)
{
    setupFonts();
    setTransparentForMouse(!isEnabled());
}

bool QnIoModuleGridOverlayContentsPrivate::PortItem::isOutput() const
{
    return m_isOutput;
}

QString QnIoModuleGridOverlayContentsPrivate::PortItem::id() const
{
    return m_id;
}

QString QnIoModuleGridOverlayContentsPrivate::PortItem::label() const
{
    return m_label;
}

void QnIoModuleGridOverlayContentsPrivate::PortItem::setLabel(const QString& label)
{
    if (m_label == label)
        return;

    m_label = label;
    m_elidedLabelWidth = kDirtyWidth;
    update();
}

bool QnIoModuleGridOverlayContentsPrivate::PortItem::isOn() const
{
    return m_on;
}

void QnIoModuleGridOverlayContentsPrivate::PortItem::setOn(bool on)
{
    if (m_on == on)
        return;

    m_on = on;
    update();
}

qreal QnIoModuleGridOverlayContentsPrivate::PortItem::minIdWidth() const
{
    return m_minIdWidth;
}

void QnIoModuleGridOverlayContentsPrivate::PortItem::setMinIdWidth(qreal value)
{
    if (qFuzzyIsNull(m_minIdWidth - value))
        return;

    m_minIdWidth = value;
    update();
}

qreal QnIoModuleGridOverlayContentsPrivate::PortItem::idWidth() const
{
    if (isWidthDirty(m_idWidth))
        m_idWidth = QFontMetricsF(m_activeIdFont).size(Qt::TextSingleLine, m_id).width();

    return m_idWidth;
}

qreal QnIoModuleGridOverlayContentsPrivate::PortItem::effectiveIdWidth() const
{
    return qMax(idWidth(), m_minIdWidth);
}

void QnIoModuleGridOverlayContentsPrivate::PortItem::setupFonts()
{
    m_idFont = font();
    m_idFont.setPixelSize(kIdFontPixelSize);
    m_idFont.setWeight(kIdFontWeight);
    m_activeIdFont = m_idFont;
    m_activeIdFont.setWeight(kActiveIdFontWeight);
    m_labelFont = font();
    m_labelFont.setPixelSize(kLabelFontPixelSize);
    m_labelFont.setWeight(kLabelFontWeight);
    m_elidedLabelWidth = kDirtyWidth;
    m_idWidth = kDirtyWidth;
}

void QnIoModuleGridOverlayContentsPrivate::PortItem::setTransparentForMouse(bool transparent)
{
    setAcceptedMouseButtons(transparent ? Qt::NoButton : Qt::AllButtons);
    setAcceptHoverEvents(!transparent);
}

void QnIoModuleGridOverlayContentsPrivate::PortItem::ensureElidedLabel(qreal width) const
{
    bool elidedLabelDirty = !qFuzzyIsNull(m_elidedLabelWidth - width);
    if (!elidedLabelDirty)
        return;

    m_elidedLabel = QFontMetricsF(m_labelFont).elidedText(m_label, Qt::ElideRight, width);
    m_elidedLabelWidth = width;
}

void QnIoModuleGridOverlayContentsPrivate::PortItem::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);
    switch (event->type())
    {
        case QEvent::FontChange:
            setupFonts();
            break;

        case QEvent::EnabledChange:
            setTransparentForMouse(!isEnabled());
            break;

        default:
            break;
    }
}

void QnIoModuleGridOverlayContentsPrivate::PortItem::paintId(
    QPainter* painter, const QRectF& rect, bool on)
{
    const auto pixmap = QnTextPixmapCache::instance()->pixmap(m_id,
        on ? m_activeIdFont : m_idFont,
        palette().color(on ? QPalette::HighlightedText : QPalette::WindowText));

    paintPixmap(painter, pixmap, rect, Qt::AlignLeft | Qt::AlignVCenter);
}

void QnIoModuleGridOverlayContentsPrivate::PortItem::paintLabel(
    QPainter* painter, const QRectF& rect, Qt::AlignmentFlag horizontalAlignment)
{
    ensureElidedLabel(rect.width());

    const auto pixmap = QnTextPixmapCache::instance()->pixmap(m_elidedLabel,
        m_labelFont, palette().color(QPalette::ButtonText));

    paintPixmap(painter, pixmap, rect, horizontalAlignment | Qt::AlignVCenter);
}

/*
QnIoModuleGridOverlayContentsPrivate::InputPortItem
*/

QnIoModuleGridOverlayContentsPrivate::InputPortItem::InputPortItem(
    const QString& id,
    QGraphicsItem* parent)
    :
    PortItem(id, false/*isOutput*/, parent)
{
}

void QnIoModuleGridOverlayContentsPrivate::InputPortItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
    QN_UNUSED(option, widget);

    auto idRect = rect().adjusted(style::Metrics::kDefaultTopLevelMargin, 0.0, 0.0, 0.0);
    paintId(painter, idRect, false);

    auto margin = style::Metrics::kStandardPadding + effectiveIdWidth();
    auto icon = qnSkin->icon(lit("io/indicator_off.png"), lit("io/indicator_on.png"));
    auto iconRect = QRectF(idRect.left() + margin, idRect.top(), kIndicatorWidth, idRect.height());
    auto iconState = isOn() ? QIcon::On : QIcon::Off;
    icon.paint(painter, iconRect.toRect(), Qt::AlignCenter, QIcon::Normal, iconState);

    margin += kIndicatorWidth;
    auto labelRect = idRect.adjusted(margin, 0.0, -style::Metrics::kDefaultTopLevelMargin, 0.0);
    paintLabel(painter, labelRect, Qt::AlignLeft);
}

/*
QnIoModuleGridOverlayContentsPrivate::OutputPortItem
*/

QnIoModuleGridOverlayContentsPrivate::OutputPortItem::OutputPortItem(
    const QString& id,
    ClickHandler clickHandler,
    QGraphicsItem* parent)
    :
    base_type(id, true/*isOutput*/, parent),
    m_clickHandler(clickHandler)
{
    setClickableButtons(Qt::LeftButton);
}

void QnIoModuleGridOverlayContentsPrivate::OutputPortItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
    QN_UNUSED(option, widget);

    auto itemRect(rect().adjusted(0.0, 0.0, -1.0, -1.0));
    bool hovered = isEnabled() && isUnderMouse();
    bool pressed = hovered && scene() && scene()->mouseGrabberItem() == this;

    auto colorRole = pressed ? QPalette::Dark : (hovered ? QPalette::Midlight : QPalette::Button);
    painter->fillRect(itemRect, palette().brush(colorRole));

    if (pressed)
    {
        /* Draw shadows: */
        constexpr qreal kTopShadowWidth = 2.0;
        constexpr qreal kSideShadowWidth = 1.0;

        auto topRect = itemRect;
        topRect.setHeight(kTopShadowWidth);
        painter->fillRect(topRect, palette().shadow());

        auto sideRect = itemRect;
        sideRect.setWidth(kSideShadowWidth);
        painter->fillRect(sideRect, palette().shadow());
        sideRect.moveRight(itemRect.right());
        painter->fillRect(sideRect, palette().shadow());
    }

    auto idRect = rect().adjusted(style::Metrics::kDefaultTopLevelMargin, 0.0, 0.0, 0.0);
    paintId(painter, idRect, isOn());

    auto margin = effectiveIdWidth() + style::Metrics::kDefaultTopLevelMargin;
    auto labelRect = idRect.adjusted(margin, 0.0, -style::Metrics::kDefaultTopLevelMargin, 0.0);
    paintLabel(painter, labelRect, Qt::AlignHCenter);
}

void QnIoModuleGridOverlayContentsPrivate::OutputPortItem::clickedNotify(QGraphicsSceneMouseEvent* event)
{
    QN_UNUSED(event);

    if (!isEnabled() || !m_clickHandler)
        return;

    m_clickHandler(id());
}

/*
QnIoModuleGridOverlayContentsPrivate::Layout
*/

QnIoModuleGridOverlayContentsPrivate::Layout::Layout(QGraphicsLayoutItem* parent) :
    QGraphicsLayout(parent),
    m_layoutDirty(false)
{
    m_rowCounts.fill(0);
}

/* Clear layout and destroy all items: */
void QnIoModuleGridOverlayContentsPrivate::Layout::clear()
{
    for (auto item: m_itemsById)
        item->deleteLater();

    m_itemsById.clear();
    m_inputItems.clear();
    m_outputItems.clear();
    invalidate();
}

/* Add new input or output item sequentially: */
bool QnIoModuleGridOverlayContentsPrivate::Layout::addItem(PortItem* item)
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
QnIoModuleGridOverlayContentsPrivate::PortItem*
    QnIoModuleGridOverlayContentsPrivate::Layout::itemById(const QString& id)
{
    auto it = m_itemsById.find(id);
    if (it != m_itemsById.end())
        return *it;

    return nullptr;
}

/* Total item count in the layout: a method required by layout engine: */
int QnIoModuleGridOverlayContentsPrivate::Layout::count() const
{
    auto count = m_inputItems.size() + m_outputItems.size();
    NX_EXPECT(count == m_itemsById.size());
    return count;
}

/* A helper method to find item location by linear index: */
QnIoModuleGridOverlayContentsPrivate::Layout::Location
    QnIoModuleGridOverlayContentsPrivate::Layout::locate(int index)
{
    NX_ASSERT(index >= 0 || index < count());

    if (index < m_inputItems.size())
        return Location(m_inputItems, index);

    return Location(m_outputItems, index - m_inputItems.size());
}

/* A helper method to find item location by linear index: */
QnIoModuleGridOverlayContentsPrivate::Layout::ConstLocation
    QnIoModuleGridOverlayContentsPrivate::Layout::locate(int index) const
{
    NX_ASSERT(index >= 0 || index < count());

    if (index < m_inputItems.size())
        return ConstLocation(m_inputItems, index);

    return ConstLocation(m_outputItems, index - m_inputItems.size());
}

/* Item by linear index: a method required by layout engine: */
QnIoModuleGridOverlayContentsPrivate::Layout::ItemWithPosition&
    QnIoModuleGridOverlayContentsPrivate::Layout::itemWithPosition(int index)
{
    auto location = locate(index);
    return location.first[location.second];
}

/* Item by linear index: a method required by layout engine: */
QGraphicsLayoutItem* QnIoModuleGridOverlayContentsPrivate::Layout::itemAt(int index) const
{
    auto location = locate(index);
    return location.first[location.second].item;
}

/* Remove item by linear index: a method required by layout engine: */
void QnIoModuleGridOverlayContentsPrivate::Layout::removeAt(int index)
{
    auto location = locate(index);
    auto item = location.first[location.second].item;
    location.first.removeAt(location.second);
    m_itemsById.remove(item->id());
    invalidate();
}

/* Invalidate layout contents: */
void QnIoModuleGridOverlayContentsPrivate::Layout::invalidate()
{
    base_type::invalidate();
    m_layoutDirty = true;
}

/* Recalculate item geometries: */
void QnIoModuleGridOverlayContentsPrivate::Layout::setGeometry(const QRectF& rect)
{
    QGraphicsLayout::setGeometry(rect);
    auto effectiveGeometry = geometry();

    ensureLayout(); //< lay out in columns and rows

    int totalItems = count();
    switch (totalItems)
    {
        /* No items: */
        case 0:
            break;

        /* One item taking entire layout: */
        case 1:
            itemAt(0)->setGeometry(effectiveGeometry);
            break;

        /* More items are always laid out in two columns: */
        default:
        {
            /* Calculate column and row dimensions: */
            qreal columnWidth = effectiveGeometry.width() / kColumnCount;
            std::array<qreal, kColumnCount> rowHeights;

            for (int i = 0; i < kColumnCount; ++i)
            {
                NX_EXPECT(m_rowCounts[i] > 0);
                rowHeights[i] = effectiveGeometry.height() / m_rowCounts[i];
            }

            /* Calculate item geometries: */
            for (int i = 0; i < totalItems; ++i)
            {
                const auto& item = itemWithPosition(i);
                qreal rowHeight = rowHeights[item.position.x()];

                QRectF itemRect(
                    effectiveGeometry.left() + columnWidth * item.position.x(),
                    effectiveGeometry.top() + rowHeight * item.position.y(),
                    columnWidth,
                    rowHeight);

                item.item->setGeometry(itemRect);
            }

            /* Align input and output id labels in each column separately: */
            for (auto list: { &m_inputItems, &m_outputItems })
            {
                /* Calculate max id label width in each column: */
                std::array<qreal, kColumnCount> idMaxWidths;
                idMaxWidths.fill(0.0);

                for (const auto& item: *list)
                {
                    auto& maxWidth = idMaxWidths[item.position.x()];
                    maxWidth = qMax(maxWidth, item.item->idWidth());
                }

                /* Set it as reserved width: */
                for (const auto& item: *list)
                    item.item->setMinIdWidth(idMaxWidths[item.position.x()]);
            }
        }
    }
}

/* Lay items by columns and rows: */
void QnIoModuleGridOverlayContentsPrivate::Layout::ensureLayout()
{
    if (!m_layoutDirty)
        return;

    const int totalItems = count();
    switch (totalItems)
    {
        /* No items: */
        case 0:
            m_rowCounts.fill(0);
            break;

        /* One item taking entire layout: */
        case 1:
            m_rowCounts[kLeftColumn] = 1;
            m_rowCounts[kRightColumn] = 0;
            itemWithPosition(0).position = QPoint(kLeftColumn, 0);
            break;

        /* More items are always laid out in two columns: */
        default:
        {
            m_rowCounts[kLeftColumn] = totalItems / 2;
            m_rowCounts[kRightColumn] = m_rowCounts[kLeftColumn];

            if (isOdd(totalItems))
            {
                if (m_inputItems.size() > m_outputItems.size())
                    ++m_rowCounts[kLeftColumn];
                else
                    ++m_rowCounts[kRightColumn];
            }

            NX_EXPECT(m_rowCounts[kLeftColumn] + m_rowCounts[kRightColumn] == totalItems);

            /* Lay out left column: */
            for (int i = 0; i < m_rowCounts[kLeftColumn]; ++i)
                itemWithPosition(i).position = QPoint(kLeftColumn, i);

            /* Lay out right column: */
            for (int i = 0; i < m_rowCounts[kRightColumn]; ++i)
                itemWithPosition(m_rowCounts[kLeftColumn] + i).position = QPoint(kRightColumn, i);
        }
    }

    m_layoutDirty = false;
}

/* Obtain size hints: */
QSizeF QnIoModuleGridOverlayContentsPrivate::Layout::sizeHint(
    Qt::SizeHint which,
    const QSizeF& constraint) const
{
    Q_UNUSED(constraint);

    if (which != Qt::MinimumSize)
        return QSizeF();

    const int totalItems = count();
    switch (totalItems)
    {
        /* No items: */
        case 0:
            return QSizeF();

        /* One item taking entire layout: */
        case 1:
            return QSizeF(kMinimumItemWidth, kMinimumItemHeight);

        /* More items are always laid out in two columns: */
        default:
        {
            const int itemsPerColumn = (totalItems + 1) / kColumnCount;
            return QSizeF(kMinimumItemWidth * kColumnCount, kMinimumItemHeight * itemsPerColumn);
        }
    }
}

/*
QnIoModuleGridOverlayContentsPrivate
*/

QnIoModuleGridOverlayContentsPrivate::QnIoModuleGridOverlayContentsPrivate(
    QnIoModuleGridOverlayContents* main)
    :
    base_type(main),
    q_ptr(main),
    layout(new Layout(main))
{
}

QnIoModuleGridOverlayContentsPrivate::PortItem*
    QnIoModuleGridOverlayContentsPrivate::createItem(const QnIOPortData& port)
{
    Q_Q(QnIoModuleGridOverlayContents);
    if (!q->overlayWidget())
        return nullptr;

    bool isOutput = port.portType == Qn::PT_Output;
    if (!isOutput && port.portType != Qn::PT_Input)
        return nullptr;

    auto clickHandler = [q](const QString& id)
    {
        emit q->userClicked(id);
    };

    /* Create new item: */
    PortItem* item;
    if (isOutput)
        item = new OutputPortItem(port.id, clickHandler);
    else
        item = new InputPortItem(port.id);

    item->setLabel(port.getName());

    layout->addItem(item);
    return item;
}

void QnIoModuleGridOverlayContentsPrivate::portsChanged(
    const QnIOPortDataList& ports,
    bool userInputEnabled)
{
    layout->clear();

    for (const auto& port: ports)
    {
        if (auto item = createItem(port))
            item->setEnabled(userInputEnabled && item->isOutput());
    }
}

void QnIoModuleGridOverlayContentsPrivate::stateChanged(
    const QnIOPortData& port,
    const QnIOStateData& state)
{
    Q_UNUSED(port); //< unused for now
    if (auto item = layout->itemById(state.id))
        item->setOn(state.isActive);
}
