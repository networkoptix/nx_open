#include "io_module_grid_overlay_contents_p.h"

#include <array>

#include <QtGui/QPainter>

#include <QtWidgets/QGraphicsScene>

#include <ui/style/helper.h>
#include <ui/style/skin.h>

namespace {

/* Private file scope constants: */

constexpr int kIdFontPixelSize = 12;
constexpr int kIdFontWeight = QFont::Normal;
constexpr int kActiveIdFontWeight = QFont::Bold;
constexpr int kLabelFontPixelSize = 13;
constexpr int kLabelFontWeight = QFont::DemiBold;

constexpr int kIndicatorWidth = 28;
constexpr int kIndicatorPadding = 2;

constexpr int kMinimumItemWidth = 160;
static const int kMinimumItemHeight = style::Metrics::kButtonHeight;

/* Private file scope functions: */

static inline bool isOdd(int value)
{
    return (value & 1) != 0;
}

static void setupFonts(
    const QFont& baseFont,
    QFont& idFont,
    QFont& activeIdFont,
    QFont& labelFont)
{
    idFont = baseFont;
    idFont.setPixelSize(kIdFontPixelSize);
    idFont.setWeight(kIdFontWeight);
    activeIdFont = idFont;
    activeIdFont.setWeight(kActiveIdFontWeight);
    labelFont = baseFont;
    labelFont.setPixelSize(kLabelFontPixelSize);
    labelFont.setWeight(kLabelFontWeight);
}

} // namespace

/*
QnIoModuleGridOverlayContentsPrivate private nested class declarations
*/

class QnIoModuleGridOverlayContentsPrivate::InputPortItem:
    public QnIoModuleGridOverlayContentsPrivate::base_type::InputPortItem
{
    using base_type = QnIoModuleOverlayContentsPrivate::InputPortItem;

public:
    using base_type::InputPortItem; //< forward constructors

protected:
    virtual void paint(QPainter* painter) override;
    virtual void setupFonts(QFont& idFont, QFont& activeIdFont, QFont& labelFont) override;
};

/* Output port item implementation: */
class QnIoModuleGridOverlayContentsPrivate::OutputPortItem:
    public QnIoModuleGridOverlayContentsPrivate::base_type::OutputPortItem
{
    using base_type = QnIoModuleOverlayContentsPrivate::OutputPortItem;

public:
    using base_type::OutputPortItem; //< forward constructors

protected:
    virtual QRectF activeRect() const override;
    virtual void paint(QPainter* painter) override;
    virtual void setupFonts(QFont& idFont, QFont& activeIdFont, QFont& labelFont) override;
};

/* Layout implementation: */
class QnIoModuleGridOverlayContentsPrivate::Layout:
    public QnIoModuleGridOverlayContentsPrivate::base_type::Layout
{
    using base_type = QnIoModuleOverlayContentsPrivate::Layout;

public:
    using base_type::Layout; //< forward constructors

    virtual void setGeometry(const QRectF& rect) override;

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which,
        const QSizeF& constraint = QSizeF()) const override;

    virtual void recalculateLayout() override;

private:
    enum Columns
    {
        kLeftColumn,
        kRightColumn,

        kColumnCount
    };

    std::array<int, kColumnCount> m_rowCounts{ { 0, 0 } };
};

/*
QnIoModuleGridOverlayContentsPrivate::InputPortItem
*/

void QnIoModuleGridOverlayContentsPrivate::InputPortItem::setupFonts(
    QFont& idFont,
    QFont& activeIdFont,
    QFont& labelFont)
{
    ::setupFonts(font(), idFont, activeIdFont, labelFont);
}

void QnIoModuleGridOverlayContentsPrivate::InputPortItem::paint(QPainter* painter)
{
    auto idRect = rect().adjusted(
         style::Metrics::kDefaultTopLevelMargin, 0.0,
        -style::Metrics::kDefaultTopLevelMargin, 0.0);

    paintId(painter, idRect, false);

    auto margin = style::Metrics::kStandardPadding + effectiveIdWidth();
    auto icon = qnSkin->icon(lit("io/indicator_off.png"), lit("io/indicator_on.png"));
    auto iconRect = QRectF(idRect.left() + margin, idRect.top(), kIndicatorWidth, idRect.height());
    auto iconState = isOn() ? QIcon::On : QIcon::Off;
    icon.paint(painter, iconRect.toRect(), Qt::AlignCenter, QIcon::Normal, iconState);

    margin += kIndicatorWidth + kIndicatorPadding;
    auto labelRect = idRect.adjusted(margin, 0.0, 0.0, 0.0);
    paintLabel(painter, labelRect, Qt::AlignLeft);
}

/*
QnIoModuleGridOverlayContentsPrivate::OutputPortItem
*/

void QnIoModuleGridOverlayContentsPrivate::OutputPortItem::setupFonts(
    QFont& idFont,
    QFont& activeIdFont,
    QFont& labelFont)
{
    ::setupFonts(font(), idFont, activeIdFont, labelFont);
}

QRectF QnIoModuleGridOverlayContentsPrivate::OutputPortItem::activeRect() const
{
    return rect(); //< entire area is clickable
}

void QnIoModuleGridOverlayContentsPrivate::OutputPortItem::paint(QPainter* painter)
{
    auto palette = this->palette();
    if (!isEnabled())
        palette.setCurrentColorGroup(QPalette::Disabled);

    auto itemRect(rect().adjusted(0.0, 0.0, -1.0, -1.0));
    bool hovered = isHovered();
    bool pressed = hovered && scene() && scene()->mouseGrabberItem() == this;

    auto colorRole = pressed ? QPalette::Dark : (hovered ? QPalette::Midlight : QPalette::Button);
    painter->fillRect(itemRect, palette.brush(colorRole));

    if (pressed)
    {
        /* Draw shadows: */
        constexpr qreal kTopShadowWidth = 2.0;
        constexpr qreal kSideShadowWidth = 1.0;

        auto topRect = itemRect;
        topRect.setHeight(kTopShadowWidth);
        painter->fillRect(topRect, palette.shadow());

        auto sideRect = itemRect;
        sideRect.setTop(topRect.bottom());
        sideRect.setWidth(kSideShadowWidth);
        painter->fillRect(sideRect, palette.shadow());
        sideRect.moveRight(itemRect.right());
        painter->fillRect(sideRect, palette.shadow());
    }

    auto idRect = rect().adjusted(style::Metrics::kDefaultTopLevelMargin, 0.0, 0.0, 0.0);
    paintId(painter, idRect, isOn());

    auto margin = effectiveIdWidth() + style::Metrics::kDefaultTopLevelMargin;
    auto labelRect = idRect.adjusted(margin, 0.0, -style::Metrics::kDefaultTopLevelMargin, 0.0);
    paintLabel(painter, labelRect, Qt::AlignHCenter);
}

/*
QnIoModuleGridOverlayContentsPrivate::Layout
*/

/* Recalculate item geometries: */
void QnIoModuleGridOverlayContentsPrivate::Layout::setGeometry(const QRectF& rect)
{
    base_type::setGeometry(rect);
    auto contents = contentsRect();

    int totalItems = count();
    switch (totalItems)
    {
        /* No items: */
        case 0:
            break;

        /* One item taking entire layout: */
        case 1:
            itemAt(0)->setGeometry(contents);
            break;

        /* More items are always laid out in two columns: */
        default:
        {
            /* Calculate column and row dimensions: */
            qreal columnWidth = contents.width() / kColumnCount;
            std::array<qreal, kColumnCount> rowHeights;

            for (int i = 0; i < kColumnCount; ++i)
            {
                NX_EXPECT(m_rowCounts[i] > 0);
                rowHeights[i] = contents.height() / m_rowCounts[i];
            }

            /* Calculate item geometries: */
            for (int i = 0; i < totalItems; ++i)
            {
                const auto& item = itemWithPosition(i);
                qreal rowHeight = rowHeights[item.position.x()];

                QRectF itemRect(
                    contents.left() + columnWidth * item.position.x(),
                    contents.top() + rowHeight * item.position.y(),
                    columnWidth,
                    rowHeight);

                item.item->setGeometry(itemRect);
            }
        }
    }
}

/* Lay items by columns and rows: */
void QnIoModuleGridOverlayContentsPrivate::Layout::recalculateLayout()
{
    const int totalItems = count();
    switch (totalItems)
    {
        /* No items: */
        case 0:
        {
            m_rowCounts.fill(0);
            break;
        }

        /* One item taking entire layout: */
        case 1:
        {
            m_rowCounts[kLeftColumn] = 1;
            m_rowCounts[kRightColumn] = 0;
            auto& item = itemWithPosition(0);
            item.position = QPoint(kLeftColumn, 0);
            item.item->setMinIdWidth(0.0);
            break;
        }

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
    base_type(main, new Layout(main)),
    q_ptr(main)
{
    layout()->setContentsMargins(0.0, 0.0, 0.0, 0.0);
}

QnIoModuleOverlayContentsPrivate::PortItem*
    QnIoModuleGridOverlayContentsPrivate::createItem(const QString& id, bool isOutput)
{
    if (!isOutput)
        return new InputPortItem(id);

    Q_Q(QnIoModuleGridOverlayContents);
    auto clickHandler = [q](const QString& id)
    {
        emit q->userClicked(id);
    };

    return new OutputPortItem(id, clickHandler);
}
