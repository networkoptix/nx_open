// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "schedule_grid_widget.h"

#include <optional>

#include <QtCore/QLocale>
#include <QtCore/QScopeGuard>
#include <QtCore/QTime>
#include <QtGui/QColor>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource_properties/schedule/schedule_cell_painter.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/common/utils/schedule.h>
#include <utils/common/scoped_painter_rollback.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr auto kRowCount = nx::vms::common::kDaysInWeek;
static constexpr auto kColumnCount = nx::vms::common::kHoursInDay;

static constexpr auto kHeaderFontWeight = QFont::Medium;

static constexpr int kVerticalHeaderLabelsMargin = 8;

QFont headerFont()
{
    QFont font;
    font.setPixelSize(fontConfig()->normal().pixelSize());
    font.setWeight(kHeaderFontWeight);
    return font;
}

static constexpr QRect kGridRange = {QPoint(), QPoint(kColumnCount - 1, kRowCount - 1)};

QRect columnRange(int column)
{
    return {QPoint(column, 0), {QPoint(column, kRowCount - 1)}};
}

QRect rowRange(int row)
{
    return {QPoint(0, row), QPoint(kColumnCount - 1, row)};
}

QRect rangeFromDiagonalCellIndexes(const QPoint& cellIndex1, const QPoint& cellIndex2)
{
    const auto topLeft = QPoint(
        std::min(cellIndex1.x(), cellIndex2.x()),
        std::min(cellIndex1.y(), cellIndex2.y()));

    const auto bottomRight = QPoint(
        std::max(cellIndex1.x(), cellIndex2.x()),
        std::max(cellIndex1.y(), cellIndex2.y()));

    return QRect(topLeft, bottomRight);
}

QMargins uniformMargins(int margin)
{
    return QMargins(margin, margin, margin, margin);
}

} // namespace

struct ScheduleGridWidget::Private
{
    ScheduleGridWidget* const q;

    // User data.
    using ColumnData = std::array<QVariant, kRowCount>;
    using GridData = std::array<ColumnData, kColumnCount>;
    GridData gridData;
    QVariant brush;

    // Data display.
    ScheduleCellPainter* cellPainter = nullptr;

    // Starts from the first day of week according to the system locale.
    std::array<Qt::DayOfWeek, nx::vms::common::kDaysInWeek> daysOfWeek;

    // Weekdays according to the system locale.
    QList<Qt::DayOfWeek> weekDays;

    // Headers strings.
    QString cornerText = ScheduleGridWidget::tr("All");
    std::array<QString, nx::vms::common::kDaysInWeek> daysStrings;
    std::array<QString, nx::vms::common::kHoursInDay> hoursStrings;

    // Size and alignment parameters.
    int cellSize = nx::style::Metrics::kScheduleGridMinimumCellSize;
    int verticalHeaderWidth() const;
    int horizontalHeaderHeigth() const;

    QRect gridRect() const;
    QRect verticalHeaderRect() const;
    QRect horizontalHeaderRect() const;
    QRect cornerRect() const;

    QRect horizontalHeaderCellRect(int colIndex) const;
    QRect verticalHeaderCellRect(int rowIndex) const;

    // Internal methods for stored data access/altering.
    QVariant cellData(const QPoint& cellIndex) const;
    void setCellData(const QPoint& cellIndex, const QVariant& value);
    void setCellsData(const QRect& cellsRange, const QVariant& value);

    // Mouse interaction parameters.
    std::optional<QPoint> mousePressPos;
    std::optional<QPoint> mouseMovePos;
    std::optional<QRect> selectionFrameRect() const;

    std::optional<QRect> selectedCellsRange;
    QPoint cellIndexFromPos(const QPoint& pos) const;
    void updateSelectedCellsRange();

    // Widget state parameters.
    bool readOnly = false;
    bool active = true;

    // Predefined colors.
    const QColor emptyCellColor = core::colorTheme()->color("dark5");
    const QColor emptyCellHoveredColor = core::colorTheme()->color("dark6");
    const QColor headerCellHoveredColor = core::colorTheme()->color("dark9");

    const QColor gridColor = core::colorTheme()->color("dark7");
    const QColor selectionFrameColor = core::colorTheme()->color("blue13");

    const QColor cornerHeaderTextColor = core::colorTheme()->color("light16");
    const QColor hourHeaderTextColor = core::colorTheme()->color("light10");
    const QColor weekdayHeaderTextColor = core::colorTheme()->color("light10");
    const QColor weekendHeaderTextColor = core::colorTheme()->color("red_core");
    const QColor cellTextColor = core::colorTheme()->color("light4");
};

std::optional<QRect> ScheduleGridWidget::Private::selectionFrameRect() const
{
    if (!mousePressPos || !mouseMovePos)
        return std::nullopt;

    return QRect(*mousePressPos, *mouseMovePos).normalized();
}

QPoint ScheduleGridWidget::Private::cellIndexFromPos(const QPoint& pos) const
{
    const auto translatedPos = pos - gridRect().topLeft();
    return QPoint(
        std::clamp(translatedPos.x() / cellSize, 0, kColumnCount - 1),
        std::clamp(translatedPos.y() / cellSize, 0, kRowCount - 1));
}

void ScheduleGridWidget::Private::updateSelectedCellsRange()
{
    if (!mousePressPos || !mouseMovePos || !gridRect().intersects(selectionFrameRect().value()))
    {
        selectedCellsRange.reset();
        return;
    }

    const auto dragDistance = (*mousePressPos - *mouseMovePos).manhattanLength();
    if (dragDistance < QApplication::startDragDistance())
    {
        selectedCellsRange.reset();
        return;
    }

    selectedCellsRange = rangeFromDiagonalCellIndexes(
        cellIndexFromPos(*mousePressPos), cellIndexFromPos(*mouseMovePos));
}

int ScheduleGridWidget::Private::verticalHeaderWidth() const
{
    QFontMetrics fontMetrics(headerFont());
    int captionsWidth = 0;
    for (const auto& dayString: daysStrings)
    {
        captionsWidth =
            std::max(captionsWidth, fontMetrics.size(Qt::TextSingleLine, dayString).width());
    }
    return captionsWidth + 2 * kVerticalHeaderLabelsMargin;
}

int ScheduleGridWidget::Private::horizontalHeaderHeigth() const
{
    return cellSize;
}

QRect ScheduleGridWidget::Private::gridRect() const
{
    return QRect(
        QPoint(verticalHeaderWidth(), horizontalHeaderHeigth()),
        QSize(cellSize * kColumnCount, cellSize * kRowCount));
}

QRect ScheduleGridWidget::Private::verticalHeaderRect() const
{
    return QRect(
        QPoint(0, horizontalHeaderHeigth()),
        QSize(verticalHeaderWidth(), cellSize * kRowCount));
}

QRect ScheduleGridWidget::Private::horizontalHeaderRect() const
{
    return QRect(
        QPoint(verticalHeaderWidth(), 0),
        QSize(cellSize * kColumnCount, horizontalHeaderHeigth()));
}

QRect ScheduleGridWidget::Private::cornerRect() const
{
    return QRect(QPoint(), QSize(verticalHeaderWidth(), horizontalHeaderHeigth()));
}

QRect ScheduleGridWidget::Private::horizontalHeaderCellRect(int columnIndex) const
{
    QRect result = horizontalHeaderRect();
    const auto offset = result.left();
    result.setLeft(columnIndex * cellSize + offset);
    result.setRight((columnIndex + 1) * cellSize + offset);
    return result;
}

QRect ScheduleGridWidget::Private::verticalHeaderCellRect(int rowIndex) const
{
    QRect result = verticalHeaderRect();
    const auto offset = result.top();
    result.setTop(rowIndex * cellSize + offset);
    result.setBottom((rowIndex + 1) * cellSize + offset);
    return result;
}

QVariant ScheduleGridWidget::Private::cellData(const QPoint& cellIndex) const
{
    return gridData[cellIndex.x()][cellIndex.y()];
}

void ScheduleGridWidget::Private::setCellData(const QPoint& cellIndex, const QVariant& value)
{
    QVariant& cellValue = gridData[cellIndex.x()][cellIndex.y()];
    if (value == cellValue)
        return;

    cellValue = value;
    q->update();

    emit q->gridDataChanged();
}

void ScheduleGridWidget::Private::setCellsData(const QRect& cellsRange, const QVariant& value)
{
    bool dataChanged = false;
    for (int col = cellsRange.left(); col <= cellsRange.right(); ++col)
    {
        for (int row = cellsRange.top(); row <= cellsRange.bottom(); ++row)
        {
            QVariant& cellValue = gridData[col][row];
            if (value == cellValue)
                continue;

            cellValue = value;
            dataChanged = true;
        }
    }
    if (dataChanged)
    {
        q->update();
        emit q->gridDataChanged();
    }
}

ScheduleGridWidget::ScheduleGridWidget(QWidget* parent):
    QWidget(parent),
    d(new Private{this})
{
    const auto locale = QLocale::system();

    d->daysOfWeek = nx::vms::common::daysOfWeek();
    d->weekDays = locale.weekdays();

    std::rotate(
        d->daysOfWeek.begin(),
        d->daysOfWeek.begin() + (locale.firstDayOfWeek() - Qt::Monday),
        d->daysOfWeek.end());

    std::transform(d->daysOfWeek.cbegin(), d->daysOfWeek.cend(), d->daysStrings.begin(),
        [&locale](const auto& day) { return locale.dayName(day, QLocale::ShortFormat); });

    QString timeFormat = locale.timeFormat(QLocale::ShortFormat);

    // Remove all separators, whitespace symbols and parts except hours from system time format.
    timeFormat
        .remove(QChar('m'), Qt::CaseInsensitive)
        .remove(QChar('s'), Qt::CaseInsensitive)
        .remove(QChar('z'), Qt::CaseInsensitive)
        .remove(QChar('t'), Qt::CaseInsensitive)
        .remove(QChar(':'))
        .remove(QChar('.'))
        .remove(QChar(' '));

    // To keep labels compact hour is represented always without leading zero.
    timeFormat
        .replace("HH", "H", Qt::CaseSensitive)
        .replace("hh", "h", Qt::CaseSensitive);

    // 'AM/PM' labels are converted to lower case, but will be rendered in the Small Caps style.
    timeFormat
        .replace("A", "a", Qt::CaseSensitive)
        .replace("P", "p", Qt::CaseSensitive);

    for (int hour = 0; hour < nx::vms::common::kHoursInDay; ++hour)
        d->hoursStrings[hour] = QTime(hour, 0).toString(timeFormat);

    setMouseTracking(true);
}

ScheduleGridWidget::~ScheduleGridWidget()
{
}

QVariant ScheduleGridWidget::brush() const
{
    return d->brush;
}

void ScheduleGridWidget::setBrush(const QVariant& value)
{
    d->brush = value;
}

void ScheduleGridWidget::setCellPainter(ScheduleCellPainter* cellPainter)
{
    if (d->cellPainter == cellPainter)
        return;

    d->cellPainter = cellPainter;
    update();
}

QVariant ScheduleGridWidget::cellData(Qt::DayOfWeek day, int hour) const
{
    int dayIndex = std::distance(
        d->daysOfWeek.cbegin(), std::find(d->daysOfWeek.cbegin(), d->daysOfWeek.cend(), day));
    return d->cellData(QPoint(hour, dayIndex));
}

void ScheduleGridWidget::setCellData(Qt::DayOfWeek day, int hour, const QVariant& value)
{
    int dayIndex = std::distance(
        d->daysOfWeek.cbegin(), std::find(d->daysOfWeek.cbegin(), d->daysOfWeek.cend(), day));
    d->setCellData(QPoint(hour, dayIndex), value);
}

bool ScheduleGridWidget::empty() const
{
    return std::all_of(
        d->gridData.begin(), d->gridData.end(),
        [](const Private::ColumnData& col)
        {
            return std::all_of(col.begin(), col.end(), [](const QVariant& v){ return !v.toBool(); });
        });
}

void ScheduleGridWidget::fillGridData(const QVariant& cellValue)
{
    d->setCellsData(kGridRange, cellValue);
}

void ScheduleGridWidget::resetGridData()
{
    fillGridData(QVariant());
}

bool ScheduleGridWidget::isReadOnly() const
{
    return d->readOnly;
}

void ScheduleGridWidget::setReadOnly(bool value)
{
    d->readOnly = value;
}

bool ScheduleGridWidget::isActive() const
{
    return d->active;
}

void ScheduleGridWidget::setActive(bool value)
{
    if (d->active == value)
        return;

    d->active = value;
    update();
}

QSize ScheduleGridWidget::minimumSizeHint() const
{
    using namespace nx::style;

    const auto minimumCellSize = Metrics::kScheduleGridMinimumCellSize;
    return {
        minimumCellSize * kColumnCount + d->verticalHeaderWidth(),
        minimumCellSize * kRowCount + minimumCellSize};
}

bool ScheduleGridWidget::hasHeightForWidth() const
{
    return true;
}

int ScheduleGridWidget::heightForWidth(int width) const
{
    using namespace nx::style;

    double verticalHeaderWidthToCellSizeRatio =
        static_cast<double>(d->verticalHeaderWidth()) / d->cellSize;
    double horizontalHeaderToCellSizeRatio =
        static_cast<double>(d->horizontalHeaderHeigth()) / d->cellSize;
    int maximumCellSize =
        static_cast<double>(width) / (verticalHeaderWidthToCellSizeRatio + kColumnCount);

    return (horizontalHeaderToCellSizeRatio + kRowCount) * std::clamp(maximumCellSize,
        Metrics::kScheduleGridMinimumCellSize,
        Metrics::kScheduleGridMaximumCellSize);
}

void ScheduleGridWidget::mouseMoveEvent(QMouseEvent* event)
{
    d->mouseMovePos = event->pos();
    d->updateSelectedCellsRange();
    update();
}

void ScheduleGridWidget::mousePressEvent(QMouseEvent* event)
{
    if (isReadOnly() || !isEnabled())
        return;

    if (event->modifiers() != Qt::NoModifier)
    {
        if (d->gridRect().contains(event->pos()))
        {
            QPoint cellIndex = d->cellIndexFromPos(event->pos());
            emit cellClicked(d->daysOfWeek[cellIndex.y()], cellIndex.x(), event->modifiers());
        }
        return;
    }

    d->mousePressPos = event->pos();
    update();
}

void ScheduleGridWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!d->mousePressPos)
        return;

    const auto connection = connect(this, &ScheduleGridWidget::gridDataChanged,
        this, &ScheduleGridWidget::gridDataEdited);
    QScopeGuard connectionGuard([this, connection]{ disconnect(connection); });

    if (d->selectedCellsRange)
    {
        d->setCellsData(*d->selectedCellsRange, brush()); //< Rectangle range selected.
        d->selectedCellsRange.reset();
    }
    else
    {
        auto cellIndex = d->cellIndexFromPos(event->pos());
        if (d->gridRect().contains(event->pos()))
        {
            d->setCellData(cellIndex, brush()); //< Single cell clicked.
        }
        else if (d->horizontalHeaderRect().contains(event->pos()))
        {
            d->setCellsData(columnRange(cellIndex.x()), brush()); //< Column header clicked.
        }
        else if (d->verticalHeaderRect().contains(event->pos()))
        {
            d->setCellsData(rowRange(cellIndex.y()), brush()); //< Row header clicked.
        }
        else if (d->cornerRect().contains(event->pos()))
        {
            fillGridData(brush()); //< Corner clicked.
        }
    }
    d->mousePressPos.reset();
    update();
}

void ScheduleGridWidget::leaveEvent(QEvent*)
{
    d->mouseMovePos.reset();
    update();
}

void ScheduleGridWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    if (!isEnabled() || !isActive())
        painter.setOpacity(style::Hints::kDisabledItemOpacity);

    // Draw corner item.
    painter.setPen(d->cornerHeaderTextColor);
    const bool cornerHovered = (isEnabled() && !isReadOnly())
        && (d->mouseMovePos && !d->selectedCellsRange && d->cornerRect().contains(*d->mouseMovePos));
    if (cornerHovered)
        painter.fillRect(d->cornerRect(), d->headerCellHoveredColor);
    painter.setFont(headerFont());
    painter.drawText(d->cornerRect(), Qt::AlignCenter, d->cornerText);

    // Draw vertical header.
    std::optional<int> verticakHeaderHoveredIndex;
    for (int row = 0; row < kRowCount; ++row)
    {
        painter.setFont(headerFont());
        painter.setPen(d->weekDays.contains(d->daysOfWeek[row])
            ? d->weekdayHeaderTextColor
            : d->weekendHeaderTextColor);

        const auto cellRect = d->verticalHeaderCellRect(row);
        const auto hovered = (isEnabled() && !isReadOnly())
            && (d->mouseMovePos && !d->selectedCellsRange && cellRect.contains(*d->mouseMovePos));
        if (hovered)
        {
            verticakHeaderHoveredIndex = row;
            painter.fillRect(cellRect, d->headerCellHoveredColor);
        }
        painter.drawText(cellRect, Qt::AlignCenter, d->daysStrings[row]);
    }

    // Draw horizontal header.
    std::optional<int> horizontalHeaderHoveredIndex;
    for (int col = 0; col < kColumnCount; ++col)
    {
        auto font = headerFont();
        font.setCapitalization(QFont::SmallCaps);
        painter.setFont(font);
        painter.setPen(d->hourHeaderTextColor);
        const auto cellRect = d->horizontalHeaderCellRect(col);
        const auto hovered = (isEnabled() && !isReadOnly())
            && (d->mouseMovePos && !d->selectedCellsRange && cellRect.contains(*d->mouseMovePos));
        if (hovered)
        {
            horizontalHeaderHoveredIndex = col;
            painter.fillRect(cellRect, d->headerCellHoveredColor);
        }
        painter.drawText(cellRect, Qt::AlignCenter, d->hoursStrings[col]);
    }

    // Draw schedule grid.
    const auto paintedCellSize = QSize(d->cellSize - 1, d->cellSize - 1);
    const auto paintedCellOrigin =
        [this](int row, int col) -> QPoint
        {
            return d->gridRect().topLeft() + QPoint(col * d->cellSize + 1, row * d->cellSize + 1);
        };

    for (int col = 0; col < kColumnCount; ++col)
    {
        for (int row = 0; row < kRowCount; ++row)
        {
            const auto cellRect = QRect(paintedCellOrigin(row, col), paintedCellSize);
            const auto data = d->selectedCellsRange && d->selectedCellsRange->contains(col, row)
                ? d->brush
                : d->cellData({col, row});

            const auto hovered = (isEnabled() && !isReadOnly())
                && ((d->selectedCellsRange && d->selectedCellsRange->contains(col, row))
                || (horizontalHeaderHoveredIndex && *horizontalHeaderHoveredIndex == col)
                || (verticakHeaderHoveredIndex && *verticakHeaderHoveredIndex == row)
                || (d->mouseMovePos && cellRect.contains(*d->mouseMovePos))
                || cornerHovered);

            d->cellPainter->paintCellBackground(&painter, cellRect, hovered, data);
            d->cellPainter->paintCellText(&painter, cellRect, data);
        }
    }

    // Draw grid lines.
    painter.setPen(d->gridColor);
    for (int col = 0; col < kColumnCount; ++col)
    {
        painter.drawLine(
            paintedCellOrigin(0, col) - QPoint(1, 1),
            paintedCellOrigin(kRowCount, col) - QPoint(1, 1));
    }
    for (int row = 0; row < kRowCount; ++row)
    {
        painter.drawLine(
            paintedCellOrigin(row, 0) - QPoint(1, 1),
            paintedCellOrigin(row, kColumnCount) - QPoint(1, 1));
    }

    // Draw selection frame.
    if (d->selectionFrameRect())
    {
        const auto extendedGridRect = d->gridRect().marginsAdded(uniformMargins(d->cellSize / 2));
        const auto frameConstraints =
            rect().marginsRemoved(uniformMargins(1)).intersected(extendedGridRect);

        d->cellPainter->paintSelectionFrame(&painter,
            (*d->selectionFrameRect()).intersected(frameConstraints));
    }
}

void ScheduleGridWidget::resizeEvent(QResizeEvent* event)
{
    using namespace nx::style;

    QWidget::resizeEvent(event);

    double verticalHeaderWidthToCellSizeRatio =
        static_cast<double>(d->verticalHeaderWidth()) / d->cellSize;
    double horizontalHeaderToCellSizeRatio =
        static_cast<double>(d->horizontalHeaderHeigth()) / d->cellSize;

    QSizeF maximumCellSizeF(event->size());
    maximumCellSizeF.rwidth() /= (verticalHeaderWidthToCellSizeRatio + kColumnCount);
    maximumCellSizeF.rheight() /= (horizontalHeaderToCellSizeRatio + kRowCount);

    d->cellSize = std::clamp(
        static_cast<int>(std::min(maximumCellSizeF.width(), maximumCellSizeF.height())),
        Metrics::kScheduleGridMinimumCellSize,
        Metrics::kScheduleGridMaximumCellSize);
}

} // namespace nx::vms::client::desktop
