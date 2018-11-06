#include "schedule_grid_widget.h"

#include <cassert>

#include <QtCore/QDate>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>

#include <client/client_settings.h>
#include <core/resource/media_resource.h>
#include <utils/common/scoped_painter_rollback.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>

#include <nx/vms/client/desktop/common/utils/stream_quality_strings.h>

namespace nx::vms::client::desktop {

namespace {

const int kTextSpacing = 6;

const qreal kMinimumLabelsFontSize = 10.0;
const qreal kMaximumLabelsFontSize = 14.0;
const qreal kMinimumGridFontSize = 7.0;
const qreal kMaximumGridFontSize = 12.0;

const qreal kFontIncreaseStep = 0.5;

static constexpr auto kStreamQualityCount = 7;

} // anonymous namespace

ScheduleGridWidget::ScheduleGridWidget(QWidget* parent):
    QWidget(parent)
{
    resetCellValues();

    QDate date(2010, 1, 1);
    date = date.addDays(1 - date.dayOfWeek());
    for (int i = 0; i < kDaysPerWeek; ++i)
    {
        m_weekDays << date.toString(lit("ddd"));
        date = date.addDays(1);
    }

    m_cornerText = tr("All");

    setMouseTracking(true);
    m_labelsFont = font();
}

ScheduleGridWidget::~ScheduleGridWidget() = default;

int ScheduleGridWidget::cellSize() const
{
    if (m_cellSize < 0)
        m_cellSize = calculateCellSize(true);

    return m_cellSize;
}

int ScheduleGridWidget::calculateCellSize(bool headersCalculated) const
{
    if (headersCalculated)
    {
        int cellWidth = qFloor((width() - 0.5 - m_gridLeftOffset) / columnCount());
        int cellHeight = qFloor((height() - 0.5 - m_gridTopOffset) / rowCount());
        return qMin(cellWidth, cellHeight);
    }

    int cellWidth = qFloor((width() - 0.5) / (columnCount() + 1));
    int cellHeight = qFloor((height() - 0.5) / (rowCount() + 1));
    return qMin(cellWidth, cellHeight);
}

void ScheduleGridWidget::initMetrics()
{
    // determine labels font size
    m_labelsFont.setPointSizeF(kMinimumLabelsFontSize);
    m_cellSize = -1;

    int desiredCellSize = calculateCellSize(false);
    do
    {
        m_gridLeftOffset = 0;
        m_gridTopOffset = 0;
        int maxWidth = 0;
        int maxHeight = 0;
        QFontMetrics metric(m_labelsFont);

        for (const QString& weekDay : m_weekDays)
        {
            QSize sz = metric.size(Qt::TextSingleLine, weekDay);
            maxWidth = qMax(maxWidth, sz.width());
            maxHeight = qMax(maxHeight, sz.height());
            m_gridLeftOffset = qMax(sz.width(), m_gridLeftOffset);
            m_gridTopOffset = qMax(sz.height(), m_gridTopOffset);
        }

        m_gridLeftOffset += kTextSpacing;
        m_gridTopOffset += kTextSpacing;
        m_labelsFont.setPointSizeF(m_labelsFont.pointSizeF() + kFontIncreaseStep);

    } while (m_gridLeftOffset < desiredCellSize*0.5 && m_labelsFont.pointSizeF() < kMaximumLabelsFontSize);

    m_gridLeftOffset = qMax(m_gridLeftOffset, desiredCellSize);
    m_gridTopOffset = qMax(m_gridTopOffset, desiredCellSize);

    m_cornerSize = QSize(m_gridLeftOffset, m_gridTopOffset);
    qreal cellSize = this->cellSize();

    qreal totalWidth = m_gridLeftOffset + (cellSize * columnCount());
    qreal space = (width() - totalWidth) * 0.5;
    if (space > 0)
        m_gridLeftOffset += space;

    // we do not need grid font if all is hidden
    if (!m_showFps && !m_showQuality)
        return;

    // determine grid font size
    m_gridFont.setPointSizeF(kMinimumGridFontSize);
    qreal maxLength = m_showFps && m_showQuality
        ? cellSize * 0.5
        : cellSize * 0.85;

    while (true)
    {
        QFontMetrics metrics(m_gridFont);

        bool tooBig = m_gridFont.pointSizeF() >= kMaximumGridFontSize;

        // checking all variants of quality string
        if (m_showQuality)
            for (int i = 0; i < kStreamQualityCount && !tooBig; i++)
                tooBig |= (metrics.width(toShortDisplayString(static_cast<Qn::StreamQuality>(i))) > maxLength);

        // checking numbers like 11, 22, .. 99
        if (m_showFps)
            for (int i = 0; i < 9 && !tooBig; i++)
                tooBig |= (metrics.width(QString::number(i * 11)) > maxLength);

        if (tooBig)
            break;

        m_gridFont.setPointSizeF(m_gridFont.pointSizeF() + kFontIncreaseStep);
    }
}

QSize ScheduleGridWidget::minimumSizeHint() const
{
    QSize sz;

    QFont font = m_labelsFont;
    font.setBold(true);
    font.setPointSizeF(kMaximumLabelsFontSize);

    QFontMetrics fm(font);
    sz = fm.size(Qt::TextSingleLine, QLatin1String("xXx"));
    sz += QSize(kTextSpacing, kTextSpacing);
    qreal cellSize = qMin(sz.width(), sz.height());
    sz = QSize(cellSize * (columnCount() + 1), cellSize * (rowCount() + 1));

    return sz.expandedTo(QApplication::globalStrut());
}

QRectF ScheduleGridWidget::horizontalHeaderCell(int x) const
{
    qreal cellSize = this->cellSize();
    return QRectF(
        m_gridLeftOffset + x * cellSize,
        m_gridTopOffset - m_cornerSize.height(),
        cellSize,
        m_cornerSize.height());
}

QRectF ScheduleGridWidget::verticalHeaderCell(int y) const
{
    qreal cellSize = this->cellSize();
    return QRectF(
        m_gridLeftOffset - m_cornerSize.width(),
        m_gridTopOffset + y * cellSize,
        m_cornerSize.width(),
        cellSize);
}

QRectF ScheduleGridWidget::cornerHeaderCell() const
{
    return QRectF(
        m_gridLeftOffset - m_cornerSize.width(),
        m_gridTopOffset - m_cornerSize.height(),
        m_cornerSize.width(),
        m_cornerSize.height());
}

void ScheduleGridWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);

    if (!isEnabled() || !m_active)
        p.setOpacity(style::Hints::kDisabledItemOpacity);

    if (!m_cornerSize.isValid())
        initMetrics();

    int cellSize = this->cellSize();

    if (m_mouseMoveCell.y() == -1)
    {
        if (m_mouseMoveCell.x() == -1)
            p.fillRect(cornerHeaderCell(), m_colors.hoveredBackground);
        else
            p.fillRect(horizontalHeaderCell(m_mouseMoveCell.x()), m_colors.hoveredBackground);
    }
    else if (m_mouseMoveCell.x() == -1)
    {
        p.fillRect(verticalHeaderCell(m_mouseMoveCell.y()), m_colors.hoveredBackground);
    }

    p.setFont(m_labelsFont);

    if (!m_cornerText.isEmpty())
    {
        p.setPen(m_colors.allLabel);
        p.drawText(QRect(0, 0, m_gridLeftOffset - kTextSpacing, m_gridTopOffset),
            Qt::AlignRight | Qt::AlignVCenter, m_cornerText);
    }

    for (int y = 0; y < rowCount(); ++y)
    {
        p.setPen(y < 5 ? m_colors.normalLabel : m_colors.weekendLabel);
        p.drawText(QRect(0, cellSize * y + m_gridTopOffset, m_gridLeftOffset - kTextSpacing, cellSize),
            Qt::AlignRight | Qt::AlignVCenter, m_weekDays[y]);
    }

    for (int x = 0; x < columnCount(); ++x)
    {
        p.setPen(m_colors.normalLabel);
        p.drawText(QRect(m_gridLeftOffset + cellSize * x, 0, cellSize, m_gridTopOffset),
            Qt::AlignCenter | Qt::AlignVCenter, QString::number(x));
    }

    p.setFont(m_gridFont);

    p.translate(m_gridLeftOffset, m_gridTopOffset);

    const QRect rect = QRect(0, 0, cellSize - 1, cellSize - 1);

    for (int x = 0; x < columnCount(); ++x)
    {
        for (int y = 0; y < rowCount(); ++y)
        {
            auto cellParams = m_gridParams[x][y];
            bool hovered = false;

            if (!m_mousePressed)
            {
                if ((x == m_mouseMoveCell.x() || m_mouseMoveCell.x() == -1) &&
                    (y == m_mouseMoveCell.y() || m_mouseMoveCell.y() == -1))
                {
                    hovered = true;
                }
            }
            else if (m_selectedCellsRect.normalized().contains(x, y))
            {
                cellParams = m_brushParams;
                hovered = true;
            }

            QnScopedPainterTransformRollback transformRollback(&p);
            p.translate(x * cellSize + 1, y * cellSize + 1);

            paintFunctions.paintCell(&p, rect, hovered, cellParams.recordingType);

            const auto suffix = qFuzzyIsNull(cellParams.bitrateMbps) ? QString() : lit("*");

            // draw text parameters
            if (cellParams.recordingType != Qn::RecordingType::never)
            {
                p.setPen(m_colors.gridLabel);
                Qn::StreamQuality quality = cellParams.quality;

                if (m_showFps)
                {
                    bool fpsValid = cellParams.recordingType == Qn::RecordingType::never || cellParams.fps > 0;
                    NX_ASSERT(fpsValid);
                    QString fps = cellParams.recordingType != Qn::RecordingType::never && cellParams.fps > 0
                        ? QString::number(cellParams.fps)
                        : lit("-");

                    if (m_showQuality)
                    {
                        p.drawText(QRectF(QPointF(0.0, 0.0), QPointF(cellSize * 0.75, cellSize *0.5)),
                            Qt::AlignCenter, fps);
                        p.drawText(QRectF(QPointF(cellSize * 0.25, cellSize * 0.5), QPointF(cellSize, cellSize)),
                            Qt::AlignCenter, toShortDisplayString(quality) + suffix);
                    }
                    else
                    {
                        p.drawText(QRectF(QPointF(0.0, 0.0), QPointF(cellSize, cellSize)),
                            Qt::AlignCenter, fps);
                    }

                }
                else if (m_showQuality)
                {
                    p.drawText(QRectF(QPointF(0.0, 0.0), QPointF(cellSize, cellSize)),
                        Qt::AlignCenter, toShortDisplayString(quality) + suffix);
                }
            }
        }
    }

    // draw grid lines
    p.setPen(m_colors.normalLabel);

    qreal w = cellSize * columnCount();
    qreal h = cellSize * rowCount();

    p.setPen(m_colors.gridLine);

    p.drawLine(0, -m_cornerSize.height(), 0, h);
    p.drawLine(-m_cornerSize.width(), 0, w, 0);

    for (int x = 1; x <= columnCount(); ++x)
        p.drawLine(cellSize * x, 0, cellSize * x, h);

    for (int y = 1; y <= rowCount(); ++y)
        p.drawLine(0, y * cellSize, w, y * cellSize);

    p.translate(-m_gridLeftOffset, -m_gridTopOffset);

    // draw selection
    if (!m_selectedRect.isEmpty())
        paintFunctions.paintSelection(&p, m_selectedRect);
}

void ScheduleGridWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    initMetrics();
}

void ScheduleGridWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_mouseMovePos = QPoint(-1, -1);
    m_mouseMoveCell = QPoint(-2, -2);
    m_selectedCellsRect = QRect();
    update();
}

void ScheduleGridWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_mouseMovePos = event->pos();
    if (m_mousePressed && (event->pos() - m_mousePressPos).manhattanLength() >= QApplication::startDragDistance())
    {
        m_selectedRect = QRect(m_mousePressPos, event->pos()).normalized();
        update();
    }
    QPoint cell = mapToGrid(event->pos(), false);
    if (cell != m_mouseMoveCell)
    {
        m_mouseMoveCell = cell;
        update();
    }
    if (m_mousePressed)
        updateSelectedCellsRect();
}

QPoint ScheduleGridWidget::mapToGrid(const QPoint& pos, bool doTruncate) const
{
    qreal cellSize = this->cellSize();
    int cellX = (pos.x() - m_gridLeftOffset) / cellSize;
    int cellY = (pos.y() - m_gridTopOffset) / cellSize;

    if (doTruncate)
        return QPoint(qBound(0, cellX, columnCount() - 1), qBound(0, cellY, rowCount() - 1));

    if (pos.x() < m_gridLeftOffset)
        cellX = -1;

    if (pos.y() < m_gridTopOffset)
        cellY = -1;

    if (cellX < columnCount() && cellY < rowCount())
        return QPoint(cellX, cellY);

    return QPoint(-2, -2);
}

void ScheduleGridWidget::mousePressEvent(QMouseEvent* event)
{
    QPoint cell = mapToGrid(event->pos(), false);
    if (event->modifiers() & Qt::AltModifier)
    {
        if (cell.x() >= 0 && cell.y() >= 0)
            emit cellActivated(cell);
        return;
    }

    if (m_readOnly)
        return;

    m_mousePressPos = event->pos();
    m_mousePressed = true;
    m_mousePressCell = cell;
    m_selectedCellsRect = QRect(cell, cell);

    update();
    setActive(true);
}

void ScheduleGridWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_mousePressed)
    {
        m_trackedChanges = false;

        QPoint mouseReleaseCell = mapToGrid(event->pos(), false);
        if (m_mousePressCell == mouseReleaseCell)
        {
            handleCellClicked(mouseReleaseCell);
        }
        else
        {
            QPoint cell1 = mapToGrid(m_mousePressPos, true);
            QPoint cell2 = mapToGrid(event->pos(), true);

            /* We cannot use QRect::normalized here, as it provides not the results we need. */
            int x1 = qMin(cell1.x(), cell2.x());
            int x2 = qMax(cell1.x(), cell2.x());
            int y1 = qMin(cell1.y(), cell2.y());
            int y2 = qMax(cell1.y(), cell2.y());

            for (int x = x1; x <= x2; ++x)
                for (int y = y1; y <= y2; ++y)
                    updateCellValueInternal(QPoint(x, y));
        }

        if (m_trackedChanges)
            emit cellValuesChanged();
    }

    m_mousePressed = false;
    m_selectedRect = QRect();
    update();
}

void ScheduleGridWidget::setShowFps(bool value)
{
    if (m_showFps == value)
        return;

    m_showFps = value;
    initMetrics();
    update();
}

void ScheduleGridWidget::setShowQuality(bool value)
{
    if (m_showQuality == value)
        return;

    m_showQuality = value;
    initMetrics();
    update();
}

void ScheduleGridWidget::handleCellClicked(const QPoint& cell)
{
    if (cell.x() == -1 && cell.y() == -1)
    {
        for (int y = 0; y < rowCount(); ++y)
            for (int x = 0; x < columnCount(); ++x)
                updateCellValueInternal(QPoint(x, y));
    }
    else if (cell.x() == -1 && isValidRow(cell.y()))
    {
        for (int x = 0; x < columnCount(); ++x)
            updateCellValueInternal(QPoint(x, cell.y()));
    }
    else if (cell.y() == -1 && isValidColumn(cell.x()))
    {
        for (int y = 0; y < rowCount(); ++y)
            updateCellValueInternal(QPoint(cell.x(), y));
    }
    else if (isValidCell(cell))
    {
        updateCellValueInternal(cell);
    }
}

void ScheduleGridWidget::updateCellValueInternal(const QPoint& cell)
{
    NX_ASSERT(isValidCell(cell));

    setCellValue(cell, m_brushParams);
    update();
}

void ScheduleGridWidget::setCellValue(const QPoint& cell, const CellParams& value)
{
    NX_ASSERT(isValidCell(cell));

    CellParams& localValue = m_gridParams[cell.x()][cell.y()];
    if (value == localValue)
        return;

    localValue = value;

    update();
    m_trackedChanges = true;

    emit cellValueChanged(cell);
}

ScheduleGridWidget::CellParams ScheduleGridWidget::brush() const
{
    return m_brushParams;
}

void ScheduleGridWidget::setBrush(const CellParams& params)
{
    m_brushParams = params;
}

ScheduleGridWidget::CellParams ScheduleGridWidget::cellValue(const QPoint& cell) const
{
    NX_ASSERT(isValidCell(cell));
    if (!isValidCell(cell))
        return CellParams();

    return m_gridParams[cell.x()][cell.y()];
}

void ScheduleGridWidget::resetCellValues()
{
    CellParams emptyParams;
    emptyParams.fps = 0;
    emptyParams.quality = Qn::StreamQuality::undefined;
    emptyParams.recordingType = Qn::RecordingType::never;

    for (int col = 0; col < columnCount(); ++col)
        for (int row = 0; row < rowCount(); ++row)
            setCellValue(QPoint(col, row), emptyParams);

    emit cellValuesChanged();
}

bool ScheduleGridWidget::isReadOnly() const
{
    return m_readOnly;
}

void ScheduleGridWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    m_readOnly = readOnly;
}

bool ScheduleGridWidget::isActive() const
{
    return m_active;
}

void ScheduleGridWidget::setActive(bool value)
{
    if (m_active == value)
        return;
    m_active = value;
    update();
}

bool ScheduleGridWidget::isValidCell(const QPoint& cell) const
{
    return isValidColumn(cell.x()) && isValidRow(cell.y());
}

bool ScheduleGridWidget::isValidRow(int row) const
{
    return row >= 0 && row < rowCount();
}

bool ScheduleGridWidget::isValidColumn(int column) const
{
    return column >= 0 && column < columnCount();
}

void ScheduleGridWidget::setMaxFps(int maxFps, int maxDualStreamFps)
{
    for (int x = 0; x < columnCount(); ++x)
    {
        for (int y = 0; y < rowCount(); ++y)
        {
            auto cell = m_gridParams[x][y];
            int fps = cell.fps;
            int value = maxFps;

            if (cell.recordingType == Qn::RecordingType::motionAndLow)
                value = maxDualStreamFps;

            if (fps > value)
            {
                cell.fps = value;
                setCellValue(QPoint(x, y), cell);
            }
        }
    }
}

int ScheduleGridWidget::getMaxFps(bool motionPlusLqOnly)
{
    int fps = 0;
    for (int x = 0; x < columnCount(); ++x)
    {
        for (int y = 0; y < rowCount(); ++y)
        {
            auto cell = m_gridParams[x][y];
            Qn::RecordingType rt = cell.recordingType;
            if (motionPlusLqOnly && rt != Qn::RecordingType::motionAndLow)
                continue;

            if (rt == Qn::RecordingType::never)
                continue;

            fps = qMax(fps, cell.fps);
        }
    }
    return fps;
}

const QnScheduleGridColors &ScheduleGridWidget::colors() const
{
    return m_colors;
}

void ScheduleGridWidget::setColors(const QnScheduleGridColors& colors)
{
    m_colors = colors;
    update();
}

void ScheduleGridWidget::updateSelectedCellsRect()
{
    QPoint topLeft = mapToGrid(m_mousePressPos, true);
    QPoint bottomRight = mapToGrid(m_mouseMovePos, true);

    if (topLeft.x() > bottomRight.x())
        qSwap(topLeft.rx(), bottomRight.rx());

    if (topLeft.y() > bottomRight.y())
        qSwap(topLeft.ry(), bottomRight.ry());

    m_selectedCellsRect = QRect(topLeft, bottomRight);
}

} // namespace nx::vms::client::desktop
