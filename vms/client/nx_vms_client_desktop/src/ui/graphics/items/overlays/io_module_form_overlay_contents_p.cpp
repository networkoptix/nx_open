// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "io_module_form_overlay_contents_p.h"

#include <array>

#include <QtGui/QColor>
#include <QtWidgets/QGraphicsScene>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

using namespace nx::vms::client::desktop;

namespace {

/* Private file scope constants: */

constexpr int kIdFontPixelSize = 12;
constexpr auto kIdFontWeight = QFont::Normal;
constexpr auto kLabelFontWeight = QFont::Medium;

constexpr int kIndicatorWidth = 28;
constexpr int kIndicatorPadding = 2;

constexpr int kButtonMargin = 12;
constexpr qreal kRoundingRadius = 2.0;
constexpr qreal kShadowThickness = 1.0;

constexpr int kMinimumItemWidth = 128; //< without side margins
static const int kItemHeight = nx::style::Metrics::kButtonHeight;

static const auto kLayoutMargins = nx::style::Metrics::kDefaultTopLevelMargins;
static const int kVerticalSpacing = nx::style::Metrics::kDefaultLayoutSpacing.height();
static const int kHorizontalSpacing = nx::style::Metrics::kDefaultTopLevelMargin * 2;

static const QColor kBasicColor = "#383838";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIoIconSubstitutions = {
    {QnIcon::Normal, {{kBasicColor, "dark8"}}},
};

/* Private file scope functions: */

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
    labelFont = baseFont;
    labelFont.setPixelSize(fontConfig()->normal().pixelSize());
    labelFont.setWeight(kLabelFontWeight);
}

static QSizeF withContentsMargins(const QGraphicsLayout* layout, const QSizeF& size)
{
    qreal leftMargin = 0.0;
    qreal topMargin = 0.0;
    qreal rightMargin = 0.0;
    qreal bottomMargin = 0.0;
    layout->getContentsMargins(&leftMargin, &topMargin, &rightMargin, &bottomMargin);
    return size + QSizeF(leftMargin + rightMargin, topMargin + bottomMargin);
}

} // namespace

/*
QnIoModuleFormOverlayContentsPrivate private nested class declarations
*/

/* Input port item implementation: */
class QnIoModuleFormOverlayContentsPrivate::InputPortItem:
    public QnIoModuleFormOverlayContentsPrivate::base_type::InputPortItem
{
    using base_type = QnIoModuleOverlayContentsPrivate::InputPortItem;

public:
    using base_type::InputPortItem; //< forward constructors

protected:
    virtual void paint(QPainter* painter) override;
    virtual void setupFonts(QFont& idFont, QFont& activeIdFont, QFont& labelFont) override;

    virtual QSizeF sizeHint(Qt::SizeHint which,
        const QSizeF& constraint = QSizeF()) const override;
};

/* Output port item implementation: */
class QnIoModuleFormOverlayContentsPrivate::OutputPortItem:
    public QnIoModuleFormOverlayContentsPrivate::base_type::OutputPortItem
{
    using base_type = QnIoModuleOverlayContentsPrivate::OutputPortItem;

public:
    using base_type::OutputPortItem; //< forward constructors

protected:
    virtual QRectF activeRect() const override;
    virtual void paint(QPainter* painter) override;
    virtual void setupFonts(QFont& idFont, QFont& activeIdFont, QFont& labelFont) override;

    virtual QSizeF sizeHint(Qt::SizeHint which,
        const QSizeF& constraint = QSizeF()) const override;

private:
    void ensurePaths(const QRectF& buttonRect);

private:
    /* Cached paint information: */
    QRectF m_lastButtonRect;
    QPainterPath m_buttonPath;
    QPainterPath m_topShadowPath;
    QPainterPath m_bottomShadowPath;
};

/* Layout implementation: */
class QnIoModuleFormOverlayContentsPrivate::Layout:
    public QnIoModuleFormOverlayContentsPrivate::base_type::Layout
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

    std::array<int, kColumnCount> m_rowCounts { { 0, 0 } };
    std::array<qreal, kColumnCount> m_widthHints { { 0.0, 0.0 } };
};

/*
QnIoModuleFormOverlayContentsPrivate::InputPortItem
*/

void QnIoModuleFormOverlayContentsPrivate::InputPortItem::setupFonts(
    QFont& idFont,
    QFont& activeIdFont,
    QFont& labelFont)
{
    ::setupFonts(font(), idFont, activeIdFont, labelFont);
}

void QnIoModuleFormOverlayContentsPrivate::InputPortItem::paint(QPainter* painter)
{
    auto idRect = rect();
    paintId(painter, idRect, false);

    auto margin = nx::style::Metrics::kStandardPadding + effectiveIdWidth();
    auto icon =
        qnSkin->icon("io/indicator_off_28.svg", kIoIconSubstitutions, "io/indicator_on_28.svg");
    auto iconRect = QRectF(idRect.left() + margin, idRect.top(), kIndicatorWidth, idRect.height());
    auto iconState = isOn() ? QIcon::On : QIcon::Off;
    icon.paint(painter, iconRect.toRect(), Qt::AlignCenter, QIcon::Normal, iconState);

    margin += kIndicatorWidth + kIndicatorPadding;
    auto labelRect = idRect.adjusted(margin, 0.0, 0.0, 0.0);
    paintLabel(painter, labelRect, Qt::AlignLeft);
}

QSizeF QnIoModuleFormOverlayContentsPrivate::InputPortItem::sizeHint(
    Qt::SizeHint which, const QSizeF& constraint) const
{
    Q_UNUSED(constraint);
    if (which != Qt::PreferredSize)
        return QSizeF();

    auto width = effectiveIdWidth()
        + nx::style::Metrics::kStandardPadding
        + kIndicatorWidth
        + kIndicatorPadding
        + labelWidth();

    return QSizeF(width, kItemHeight);
}

/*
QnIoModuleFormOverlayContentsPrivate::OutputPortItem
*/

void QnIoModuleFormOverlayContentsPrivate::OutputPortItem::setupFonts(
    QFont& idFont,
    QFont& activeIdFont,
    QFont& labelFont)
{
    ::setupFonts(font(), idFont, activeIdFont, labelFont);
}

QRectF QnIoModuleFormOverlayContentsPrivate::OutputPortItem::activeRect() const
{
    QRectF buttonRect = rect();
    buttonRect.setLeft(buttonRect.left()
        + nx::style::Metrics::kStandardPadding
        + effectiveIdWidth());
    return buttonRect;
}

void QnIoModuleFormOverlayContentsPrivate::OutputPortItem::ensurePaths(const QRectF& buttonRect)
{
    if (buttonRect == m_lastButtonRect) //< non-fuzzy compare is fine here
        return;

    m_buttonPath = QPainterPath();
    m_buttonPath.addRoundedRect(buttonRect, kRoundingRadius, kRoundingRadius);

    auto shiftedDownPath = QPainterPath();
    auto shiftedDownRect = buttonRect.adjusted(0.0, kShadowThickness, 0.0, kShadowThickness);
    shiftedDownPath.addRoundedRect(shiftedDownRect, kRoundingRadius, kRoundingRadius);

    auto shiftedUpPath = QPainterPath();
    auto shiftedUpRect = buttonRect.adjusted(0.0, -kShadowThickness, 0.0, -kShadowThickness);
    shiftedUpPath.addRoundedRect(shiftedUpRect, kRoundingRadius, kRoundingRadius);

    m_topShadowPath = m_buttonPath - shiftedDownPath;
    m_bottomShadowPath = m_buttonPath - shiftedUpPath;

    m_lastButtonRect = buttonRect;
}

void QnIoModuleFormOverlayContentsPrivate::OutputPortItem::paint(QPainter* painter)
{
    auto palette = this->palette();
    if (!isEnabled())
        palette.setCurrentColorGroup(QPalette::Disabled);

    auto idRect = rect();
    paintId(painter, idRect, false);

    bool hovered = isHovered();
    bool pressed = hovered && scene() && scene()->mouseGrabberItem() == this;
    auto colorRole = pressed ? QPalette::Dark : (hovered ? QPalette::Midlight : QPalette::Button);
    auto buttonRect = activeRect();

    ensurePaths(buttonRect);
    painter->fillPath(m_buttonPath, palette.brush(colorRole));

    const QPainterPath& shadowPath = pressed ? m_topShadowPath : m_bottomShadowPath;
    painter->fillPath(shadowPath, palette.shadow());

    auto labelRect = buttonRect.adjusted(kButtonMargin, 0.0, -kIndicatorWidth, 0.0);
    paintLabel(painter, labelRect, Qt::AlignLeft);

    auto icon = qnSkin->icon(
        "io/button_indicator_off_24.svg", kIoIconSubstitutions, "io/button_indicator_on_24.svg");
    auto iconState = isOn() ? QIcon::On : QIcon::Off;
    auto iconRect = buttonRect;
    iconRect.setLeft(iconRect.right() - kIndicatorWidth);
    icon.paint(painter, iconRect.toRect(), Qt::AlignCenter, QIcon::Normal, iconState);
}

QSizeF QnIoModuleFormOverlayContentsPrivate::OutputPortItem::sizeHint(
    Qt::SizeHint which, const QSizeF& constraint) const
{
    Q_UNUSED(constraint);
    if (which != Qt::PreferredSize)
        return QSizeF();

    auto width = effectiveIdWidth()
        + nx::style::Metrics::kStandardPadding
        + kButtonMargin
        + labelWidth()
        + kIndicatorWidth;

    return QSizeF(width, kItemHeight);
}

/*
QnIoModuleFormOverlayContentsPrivate::Layout
*/

/* Recalculate item geometries: */
void QnIoModuleFormOverlayContentsPrivate::Layout::setGeometry(const QRectF& rect)
{
    base_type::setGeometry(rect);
    auto contents = contentsRect();

    int totalItems = count();
    if (totalItems == 0)
        return;

    /* Calculate column widths: */
    std::array<qreal, kColumnCount> columnWidths = { 0.0, 0.0 };

    if (m_rowCounts[kRightColumn] == 0)
    {
        /* One-column layout: */
        columnWidths[kLeftColumn] = qMin(contents.width(), m_widthHints[kLeftColumn]);
    }
    else
    {
        /* Two-column layout: */
        auto space = contents.width() - kHorizontalSpacing; //< content space for two columns
        auto half = space / 2.0;

        enum
        {
            kBothNarrow = 0,
            kLeftWide  = 0x1,
            kRightWide = 0x2,
            kBothWide = kLeftWide | kRightWide
        };

        int state = kBothNarrow;
        if (m_widthHints[kLeftColumn] > half)
            state |= kLeftWide;
        if (m_widthHints[kRightColumn] > half)
            state |= kRightWide;

        /* Column cannot take less space than kMinimumItemWidth
         * if there's enough space for another column,
         * but can be shrinked to its width hint otherwise: */
        switch (state)
        {
            case kBothNarrow:
                for (int i = 0; i < kColumnCount; ++i)
                    columnWidths[i] = qMax<qreal>(m_widthHints[i], kMinimumItemWidth);
                break;

            case kBothWide:
                columnWidths[kLeftColumn] = columnWidths[kRightColumn] = half;
                break;

            case kLeftWide:
                columnWidths[kRightColumn] = m_widthHints[kRightColumn];
                columnWidths[kLeftColumn] = qMin(m_widthHints[kLeftColumn],
                    space - columnWidths[kRightColumn]);
                break;

            case kRightWide:
                columnWidths[kLeftColumn] = m_widthHints[kLeftColumn];
                columnWidths[kRightColumn] = qMin(m_widthHints[kRightColumn],
                    space - columnWidths[kLeftColumn]);
                break;
        }
    }

    qreal rowStride = kItemHeight + kVerticalSpacing;
    const std::array<qreal, kColumnCount> columnPositions = { 0.0,
        columnWidths[kLeftColumn] + kHorizontalSpacing };

    /* Calculate item geometries: */
    for (int i = 0; i < totalItems; ++i)
    {
        const auto& item = itemWithPosition(i);

        QRectF itemRect(
            contents.left() + columnPositions[item.position.x()],
            contents.top() + rowStride * item.position.y(),
            columnWidths[item.position.x()],
            kItemHeight);

        item.item->setGeometry(itemRect);
    }
}

/* Lay items by columns and rows: */
void QnIoModuleFormOverlayContentsPrivate::Layout::recalculateLayout()
{
    m_rowCounts.fill(0);
    m_widthHints.fill(0.0);

    if (!count())
        return;

    /* Layout columns: */
    int column = kLeftColumn;
    for (auto list: { &m_inputItems, &m_outputItems })
    {
        if (list->empty())
            continue;

        qreal maxIdWidth = 0.0;
        m_rowCounts[column] = list->size();

        int row = 0;
        for (auto& item : *list)
        {
            item.position = Position(column, row++);
            maxIdWidth = qMax(maxIdWidth, item.item->idWidth());
        }

        for (auto& item: *list)
        {
            item.item->setMinIdWidth(maxIdWidth);
            m_widthHints[column] = qMax(m_widthHints[column],
                item.item->effectiveSizeHint(Qt::PreferredSize).width());
        }

        ++column;
    }
}

/* Obtain size hints: */
QSizeF QnIoModuleFormOverlayContentsPrivate::Layout::sizeHint(
    Qt::SizeHint which,
    const QSizeF& constraint) const
{
    Q_UNUSED(constraint);

    if (which != Qt::MinimumSize)
        return QSizeF();

    if (count() == 0)
        return QSizeF();

    const int rowCount = qMax(m_inputItems.size(), m_outputItems.size());
    const int height = kItemHeight * rowCount + kVerticalSpacing * (rowCount - 1);
    const int width = m_inputItems.empty() || m_outputItems.empty()
        ? kMinimumItemWidth                           //< one-column layout
        : kMinimumItemWidth * 2 + kHorizontalSpacing; //< two-column layout

    return withContentsMargins(this, QSizeF(width, height));
}

/*
QnIoModuleFormOverlayContentsPrivate
*/

QnIoModuleFormOverlayContentsPrivate::QnIoModuleFormOverlayContentsPrivate(
    QnIoModuleFormOverlayContents* main)
    :
    base_type(main, new Layout(main)),
    q_ptr(main)
{
    layout()->setContentsMargins(
        kLayoutMargins.left(),
        kLayoutMargins.top(),
        kLayoutMargins.right(),
        kLayoutMargins.bottom());
}

QnIoModuleOverlayContentsPrivate::PortItem*
    QnIoModuleFormOverlayContentsPrivate::createItem(const QString& id, bool isOutput)
{
    if (!isOutput)
        return new InputPortItem(id);

    Q_Q(QnIoModuleFormOverlayContents);
    auto clickHandler = [q](const QString& id)
    {
        emit q->userClicked(id);
    };

    return new OutputPortItem(id, clickHandler);
}
