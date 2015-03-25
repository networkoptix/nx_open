#include "schedule_grid_widget.h"

#include <cassert>

#include <QtCore/QDate>
#include <QtWidgets/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>

#include "ui/style/globals.h"
#include "client/client_settings.h"
#include "utils/math/color_transformations.h"
#include <utils/math/linear_combination.h>
#include "core/resource/media_resource.h"

namespace {

    const int WEEK_SIZE = 7;
    const int TEXT_SPACING = 4;

    const qreal minimumLabelsFontSize = 10.0;
    const qreal maximumLabelsFontSize = 14.0;
    const qreal minimumGridFontSize = 7.0;
    const qreal maximumGridFontSize = 12.0;

    const qreal fontIncreaseStep = 0.5;

    const int highlightFactor = 120;

} // anonymous namespace

QnScheduleGridWidget::QnScheduleGridWidget(QWidget *parent)
    : QWidget(parent)
{
    m_mouseMoveCell = QPoint(-2, -2);
    m_enabled = true;
    m_readOnly = false;
    m_defaultParams[FpsParam] = 10;
    m_defaultParams[QualityParam] = Qn::QualityNormal;
    m_defaultParams[RecordTypeParam] = Qn::RT_Always;

    updateCellColors();

    resetCellValues();

    QDate date(2010,1,1);
    date = date.addDays(1 - date.dayOfWeek());
    for (int i = 0; i < WEEK_SIZE; ++i) {
        m_weekDays << date.toString(QLatin1String("ddd"));
        date = date.addDays(1);
    }

    m_cornerText = tr("", "SCHEDULE_GRID_CORNER_TEXT");

    m_gridLeftOffset = 0;
    m_gridTopOffset = 0;
    m_mousePressed = false;
    setMouseTracking(true);

    m_showFps = true;
    m_showQuality = true;

    m_labelsFont = font();
}

QnScheduleGridWidget::~QnScheduleGridWidget() {
    return;
}

int QnScheduleGridWidget::cellSize() const {
    int cellWidth = qFloor((width() - 0.5 - m_gridLeftOffset) / columnCount());
    int cellHeight = qFloor((height() - 0.5 - m_gridTopOffset) / rowCount());
    return qMin(cellWidth, cellHeight);
}

void QnScheduleGridWidget::initMetrics() {
    // determine labels font size
    m_labelsFont.setBold(true);
    m_labelsFont.setPointSizeF(minimumLabelsFontSize);

    do {
        m_gridLeftOffset = 0;
        m_gridTopOffset = 0;
        int maxWidth = 0;
        int maxHeight = 0;
        QFontMetrics metric(m_labelsFont);
        foreach (const QString &weekDay, m_weekDays) {
            QSize sz = metric.size(Qt::TextSingleLine, weekDay);
            maxWidth = qMax(maxWidth, sz.width());
            maxHeight = qMax(maxHeight, sz.height());
            m_gridLeftOffset = qMax(sz.width(), m_gridLeftOffset);
            m_gridTopOffset = qMax(sz.height(), m_gridTopOffset);
        }
        m_cornerSize = QSize(maxWidth + TEXT_SPACING, maxHeight + TEXT_SPACING);
        m_gridLeftOffset += TEXT_SPACING;
        m_gridTopOffset += TEXT_SPACING;
        m_labelsFont.setPointSizeF(m_labelsFont.pointSizeF() + fontIncreaseStep);
    } while (m_gridLeftOffset < cellSize()*0.5 && m_labelsFont.pointSizeF() < maximumLabelsFontSize);

    qreal cellSize = this->cellSize();

    qreal totalWidth = m_gridLeftOffset + (cellSize * columnCount());
    qreal space = (width() - totalWidth) * 0.5;
    if (space > 0)
        m_gridLeftOffset += space;

    // we do not need grid font if all is hidden
    if (!m_showFps && !m_showQuality)
        return;

    // determine grid font size
    m_gridFont.setPointSizeF(minimumGridFontSize);
    qreal maxLength = m_showFps && m_showQuality
            ? cellSize * .4545
            : cellSize * .85;

    while (true) {
        QFontMetrics metrics(m_gridFont);

        bool tooBig = m_gridFont.pointSizeF() >= maximumGridFontSize;

        // checking all variants of quality string
        if (m_showQuality)
            for (int i = 0; i < Qn::StreamQualityCount && !tooBig; i++)
                tooBig |= (metrics.width(Qn::toShortDisplayString(static_cast<Qn::StreamQuality>(i))) > maxLength);

        // checking numbers like 11, 22, .. 99
        if (m_showFps)
            for (int i = 0; i < 9 && !tooBig; i++)
                tooBig |= (metrics.width(QString::number(i*11)) > maxLength);

        if (tooBig)
            break;
        m_gridFont.setPointSizeF(m_gridFont.pointSizeF() + fontIncreaseStep);
    }
}

QSize QnScheduleGridWidget::minimumSizeHint() const {
    QSize sz;

    QFont font = m_labelsFont;
    font.setBold(true);
    font.setPointSizeF(maximumLabelsFontSize);

    QFontMetrics fm(font);
    sz = fm.size(Qt::TextSingleLine, QLatin1String("xXx"));
    sz += QSize(TEXT_SPACING, TEXT_SPACING);
    qreal cellSize = qMin(sz.width(), sz.height());
    sz = QSize(cellSize * (columnCount() + 1), cellSize * (rowCount() + 1));

    return sz.expandedTo(QApplication::globalStrut());
}

void QnScheduleGridWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (!m_cornerSize.isValid())
        initMetrics();
    qreal cellSize = this->cellSize();

    p.setFont(m_labelsFont);

    if(!m_cornerText.isEmpty()) {
        QColor penClr;
        if (!m_enabled)
            penClr = m_colors.disabledLabel;
        else if (m_mouseMoveCell.x() == -1 && m_mouseMoveCell.y() == -1)
            penClr = m_enabled ? m_colors.selectedLabel : m_colors.normalLabel;
        else 
            penClr = m_colors.normalLabel;
        p.setPen(penClr);
        p.drawText(QRect(0, 0, m_gridLeftOffset-TEXT_SPACING, m_gridTopOffset), Qt::AlignRight | Qt::AlignVCenter, m_cornerText);
    }

    for (int y = 0; y < rowCount(); ++y) 
    {
        QColor penClr;
        if (!m_enabled)
            penClr = m_colors.disabledLabel;
        else if (m_mouseMoveCell.x() == -1 && m_mouseMoveCell.y() == y)
            penClr = m_enabled ? m_colors.selectedLabel : m_colors.normalLabel;
        else if (y < 5)
            penClr = m_colors.normalLabel;
        else
            penClr = m_colors.weekendLabel;
        p.setPen(penClr);
        p.drawText(QRect(0, cellSize*y+m_gridTopOffset, m_gridLeftOffset-TEXT_SPACING, cellSize), Qt::AlignRight | Qt::AlignVCenter, m_weekDays[y]);
    }
    for (int x = 0; x < columnCount(); ++x) 
    {
        QColor penClr;
        if (!m_enabled)
            penClr = m_colors.disabledLabel;
        else if (m_mouseMoveCell.y() == -1 && m_mouseMoveCell.x() == x)
            penClr = m_enabled ? m_colors.selectedLabel : m_colors.normalLabel;
        else
            penClr = m_colors.normalLabel;
        p.setPen(penClr);
        p.drawText(QRect(m_gridLeftOffset + cellSize*x, 0, cellSize, m_gridTopOffset), Qt::AlignCenter | Qt::AlignVCenter, QString::number(x));
    }


    p.setFont(m_gridFont);

    p.translate(m_gridLeftOffset, m_gridTopOffset);

    // draw grid colors/text
    qreal trOffset = cellSize / 6;
    qreal trSize = cellSize / 10;

    QPointF points[6];
    points[0] = QPointF(cellSize - trOffset - trSize, trOffset);
    points[1] = QPointF(cellSize - trOffset, trOffset);
    points[2] = QPointF(cellSize - trOffset, trOffset + trSize);
    points[3] = QPointF(trOffset + trSize, cellSize - trOffset);
    points[4] = QPointF(trOffset, cellSize - trOffset);
    points[5] = QPointF(trOffset, cellSize - trSize - trOffset);

    for (int x = 0; x < columnCount(); ++x)
    {
        for (int y = 0; y < rowCount(); ++y)
        {
            uint recordTypeIdx(m_gridParams[x][y][RecordTypeParam].toUInt());

            QColor color(m_cellColors[recordTypeIdx]);
            QColor colorInside(m_insideColors[recordTypeIdx]);
            if (!m_enabled) {
                color = disabledCellColor(color);
                colorInside = disabledCellColor(colorInside);
            } else {
                if (!m_mousePressed) {
                    if ((x == m_mouseMoveCell.x() || m_mouseMoveCell.x() == -1) &&
                        (y == m_mouseMoveCell.y() || m_mouseMoveCell.y() == -1))
                    {
                        color = color.lighter(highlightFactor);
                        colorInside = colorInside.lighter(highlightFactor);
                    }
                } else if (m_selectedCellsRect.normalized().contains(x, y)) {
                    uint type = m_defaultParams[RecordTypeParam].toUInt();
                    color = m_cellColors[type].lighter(highlightFactor);
                    colorInside = m_insideColors[type].lighter(highlightFactor);
                }
            }

            QTransform transform = p.transform();
            p.translate(x * cellSize, y * cellSize);

            p.fillRect(0.0, 0.0, cellSize, cellSize, color);

            {
                QColor penClr(toTransparent(m_enabled ? m_colors.normalLabel : m_colors.disabledLabel, 0.5));
                p.setPen(penClr);

                if (m_showFps && m_showQuality)
                    p.drawLine(1, cellSize - 1, cellSize - 1, 1);

                if (colorInside.toRgb() != color.toRgb()) {
                    p.setBrush(m_enabled ? colorInside : disabledCellColor(colorInside));
                    QColor penClr(toTransparent(m_enabled ? m_colors.disabledLabel : m_colors.normalLabel, 0.5));
                    p.setPen(penClr);
                    p.drawPolygon(points, 6);
                }
            }

            // draw text parameters
            {
                QColor penClr(m_enabled ? m_colors.normalLabel : m_colors.disabledLabel);

                p.setPen(penClr);
                Qn::StreamQuality quality = (Qn::StreamQuality) m_gridParams[x][y][QualityParam].toInt();
                if (m_showFps && m_showQuality)
                {
                    p.drawText(QRectF(QPointF(0.0, 0.0), QPointF(cellSize * 0.75, cellSize *0.5)), Qt::AlignCenter, m_gridParams[x][y][FpsParam].toString());
                    p.drawText(QRectF(QPointF(cellSize * 0.25, cellSize * 0.5), QPointF(cellSize, cellSize)), Qt::AlignCenter, toShortDisplayString(quality));
                }
                else if (m_showFps)
                    p.drawText(QRectF(QPointF(0.0, 0.0), QPointF(cellSize, cellSize)), Qt::AlignCenter, m_gridParams[x][y][FpsParam].toString());
                else if (m_showQuality)
                    p.drawText(QRectF(QPointF(0.0, 0.0), QPointF(cellSize, cellSize)), Qt::AlignCenter, toShortDisplayString(quality));
            }

            p.setTransform(transform);
        }
    }

    // draw grid lines
    p.setPen(m_enabled ? m_colors.normalLabel : m_colors.disabledLabel);

    if (m_cornerText.isEmpty() && m_mouseMoveCell.x() == -1 && m_mouseMoveCell.y() == -1)
        p.fillRect(-m_cornerSize.width(), -m_cornerSize.height(), m_cornerSize.width(), m_cornerSize.height(), m_colors.selectedLabel);

    qreal w = cellSize * columnCount();
    qreal h = cellSize * rowCount();

    p.drawLine(0, -m_cornerSize.height(), 0, h);
    p.drawLine(-m_cornerSize.width(), 0, w, 0);

    for (int x = 1; x <= columnCount(); ++x)
        p.drawLine(cellSize * x, 0, cellSize * x, h);
    for (int y = 1; y <= rowCount(); ++y)
        p.drawLine(0, y * cellSize, w, y * cellSize);

    p.translate(-m_gridLeftOffset, -m_gridTopOffset);

    // draw selection
    if (!m_selectedRect.isEmpty()) {
        QColor defColor = m_cellColors[m_defaultParams[RecordTypeParam].toUInt()];
        QColor brushColor = subColor(defColor, qnGlobals->selectionOpacityDelta());
        if (!m_enabled)
            brushColor = disabledCellColor(brushColor);

        p.setPen(m_enabled ? subColor(defColor, qnGlobals->selectionBorderDelta()) : m_colors.disabledLabel);
        p.setBrush(brushColor);
        p.drawRect(m_selectedRect);
    }
}

void QnScheduleGridWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    initMetrics();
}

void QnScheduleGridWidget::leaveEvent(QEvent *)
{
    m_mouseMovePos = QPoint(-1,-1);
    m_mouseMoveCell = QPoint(-2, -2);
    m_selectedCellsRect = QRect();
    update();
}

void QnScheduleGridWidget::mouseMoveEvent(QMouseEvent *event)
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

QPoint QnScheduleGridWidget::mapToGrid(const QPoint &pos, bool doTruncate) const
{
    qreal cellSize = this->cellSize();
    int cellX = (pos.x() - m_gridLeftOffset)/cellSize;
    int cellY = (pos.y() - m_gridTopOffset)/cellSize;

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

void QnScheduleGridWidget::mousePressEvent(QMouseEvent *event)
{
    QPoint cell = mapToGrid(event->pos(), false);
    if (event->modifiers() & Qt::AltModifier)
    {
        if (cell.x() >= 0 && cell.y() >= 0)
            emit cellActivated(cell);
        return;
    }
    
    if(m_readOnly)
        return;

    m_mousePressPos = event->pos();
    m_mousePressed = true;
    m_mousePressCell = cell;
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

    m_selectedCellsRect = QRect(cell, cell);

    update();
    setEnabled(true);
}

void QnScheduleGridWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_mousePressed)
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
                updateCellValueInternal(QPoint(x,y));
    }
    m_mousePressed = false;
    m_selectedRect = QRect();
    update();
}

void QnScheduleGridWidget::setDefaultParam(ParamType number, const QVariant& value)
{
    m_defaultParams[number] = value;
}

void QnScheduleGridWidget::setShowFps(bool value) {
    if (m_showFps == value)
        return;

    m_showFps = value;
    initMetrics();
    update();
}

void QnScheduleGridWidget::setShowQuality(bool value) {
    if (m_showQuality == value)
        return;

    m_showQuality = value;
    initMetrics();
    update();
}

QVariant QnScheduleGridWidget::cellValue(const QPoint &cell, ParamType paramType) const
{
    if (!isValidCell(cell))
        return QVariant();
    if (paramType < 0 || paramType >= ParamCount)
        return QVariant();
    return m_gridParams[cell.x()][cell.y()][paramType];
}

void QnScheduleGridWidget::setCellValue(const QPoint &cell, ParamType paramType, const QVariant &value)
{
    if (!isValidCell(cell))
        return;
    if (paramType < 0 || paramType >= ParamCount)
        return;
    
    QVariant &localValue = m_gridParams[cell.x()][cell.y()][paramType];
    if(localValue == value)
        return;

    localValue = value;

    emit cellValueChanged(cell);

    update();
}

Qn::RecordingType QnScheduleGridWidget::cellRecordingType(const QPoint &cell) const {
    QVariant value = cellValue(cell, RecordTypeParam);
    if (value.isValid())
        return Qn::RecordingType(value.toUInt());
    return Qn::RT_Always;
}

void QnScheduleGridWidget::setCellRecordingType(const QPoint &cell, const Qn::RecordingType &value) {
    setCellValue(cell, RecordTypeParam, value);
}

void QnScheduleGridWidget::updateCellValueInternal(const QPoint& cell)
{
    assert(isValidCell(cell));

    setCellValueInternal(cell, m_defaultParams);
    update();
}

void QnScheduleGridWidget::setCellValueInternal(const QPoint &cell, const CellParams &value) 
{
    assert(isValidCell(cell));

    CellParams &localValue = m_gridParams[cell.x()][cell.y()];
    if(qEqual(value, &value[ParamCount], localValue)) {
        return;
    }

    qCopy(value, &value[ParamCount], localValue);

    emit cellValueChanged(cell);
}

void QnScheduleGridWidget::setCellValueInternal(const QPoint &cell, ParamType type, const QVariant &value) 
{
    assert(isValidCell(cell));
    assert(type >= 0 && type < ParamCount);

    CellParams &localValue = m_gridParams[cell.x()][cell.y()];
    if(localValue[type] == value)
        return;

    localValue[type] = value;

    emit cellValueChanged(cell);
}

void QnScheduleGridWidget::resetCellValues() {
    CellParams emptyParams;
    emptyParams[FpsParam] = QLatin1String("-");
    emptyParams[QualityParam] = Qn::QualityNotDefined;
    emptyParams[RecordTypeParam] = Qn::RT_Never;

    for (int col = 0; col < columnCount(); ++col)
        for (int row = 0; row < rowCount(); ++row)
            setCellValueInternal(QPoint(col, row), emptyParams);
}

void QnScheduleGridWidget::setEnabled(bool val)
{
    //QWidget::setEnabled(val);
    m_enabled = val;
    repaint();
}

bool QnScheduleGridWidget::isEnabled() const
{
    return m_enabled;
}

bool QnScheduleGridWidget::isReadOnly() const {
    return m_readOnly;
}

void QnScheduleGridWidget::setReadOnly(bool readOnly) {
    if(m_readOnly == readOnly)
        return;

    m_readOnly = readOnly;
}

bool QnScheduleGridWidget::isValidCell(const QPoint &cell) const {
    return isValidColumn(cell.x()) && isValidRow(cell.y());
}

bool QnScheduleGridWidget::isValidRow(int row) const {
    return row >= 0 && row < rowCount();
}

bool QnScheduleGridWidget::isValidColumn(int column) const {
    return column >= 0 && column < columnCount();
}

void QnScheduleGridWidget::setMaxFps(int maxFps, int maxDualStreamFps)
{
    for (int x = 0; x < columnCount(); ++x)
    {
        for (int y = 0; y < rowCount(); ++y)
        {
            int fps = m_gridParams[x][y][FpsParam].toInt();
            int value = maxFps;
            if (m_gridParams[x][y][RecordTypeParam] == Qn::RT_MotionAndLowQuality)
                value = maxDualStreamFps;
            if(fps > value)
                setCellValueInternal(QPoint(x, y), FpsParam, value);
        }
    }
}

int QnScheduleGridWidget::getMaxFps(bool motionPlusLqOnly)
{
    int fps = 0;
    for (int x = 0; x < columnCount(); ++x) {
        for (int y = 0; y < rowCount(); ++y) {
            Qn::RecordingType rt = static_cast<Qn::RecordingType>(m_gridParams[x][y][RecordTypeParam].toUInt());
            if (motionPlusLqOnly && rt != Qn::RT_MotionAndLowQuality)
                continue;
            if (rt == Qn::RT_Never)
                continue;
            fps = qMax(fps, m_gridParams[x][y][FpsParam].toInt());
        }
    }
    return fps;
}

const QnScheduleGridColors &QnScheduleGridWidget::colors() const {
    return m_colors;
}

void QnScheduleGridWidget::setColors(const QnScheduleGridColors &colors) {
    m_colors = colors;
    updateCellColors();

    update();

    emit colorsChanged();
}

QColor QnScheduleGridWidget::disabledCellColor(const QColor &baseColor) const {
    return toGrayscale(linearCombine(0.5, baseColor, 0.5, palette().color(QPalette::Disabled, QPalette::Background)));
}

void QnScheduleGridWidget::updateSelectedCellsRect() {
    QPoint topLeft = mapToGrid(m_mousePressPos, true);
    QPoint bottomRight = mapToGrid(m_mouseMovePos, true);

    if (topLeft.x() > bottomRight.x())
        qSwap(topLeft.rx(), bottomRight.rx());
    if (topLeft.y() > bottomRight.y())
        qSwap(topLeft.ry(), bottomRight.ry());

    m_selectedCellsRect = QRect(topLeft, bottomRight);
}

void QnScheduleGridWidget::updateCellColors() {
    m_insideColors[Qn::RT_Never] = m_cellColors[Qn::RT_Never] = m_colors.recordNever;
    m_insideColors[Qn::RT_MotionOnly] = m_cellColors[Qn::RT_MotionOnly] = m_colors.recordMotion;
    m_insideColors[Qn::RT_Always] = m_cellColors[Qn::RT_Always] = m_colors.recordAlways;

    m_cellColors[Qn::RT_MotionAndLowQuality] = m_colors.recordMotion;
    m_insideColors[Qn::RT_MotionAndLowQuality] = m_colors.recordAlways;
}
