#include "schedule_grid_widget.h"

#include <cassert>

#include <QtGui/QApplication>
#include <QtGui/QPainter>

#include "ui/style/globals.h"
#include "utils/settings.h"
#include "ui/common/color_transformations.h"

namespace {

    const QColor NORMAL_LABEL_COLOR(255,255,255);
    const QColor WEEKEND_LABEL_COLOR(255,128,128);
    QColor SELECTED_LABEL_COLOR(64,128, 192);
    const int DISABLED_COLOR_SHIFT = -72;

} // anonymous namespace

QnScheduleGridWidget::QnScheduleGridWidget(QWidget *parent)
    : QWidget(parent)
{
    m_mouseMoveCell = QPoint(-2, -2);
    m_enabled = true;
    m_readOnly = false;
    m_defaultParams[FirstParam] = 10;
    m_defaultParams[SecondParam] = QLatin1String("Md");
    m_defaultParams[ColorInsideParam] = m_defaultParams[ColorParam] = qnGlobals->recordAlwaysColor().rgba();
    resetCellValues();


    QDate date(2010,1,1);
    date = date.addDays(1 - date.dayOfWeek());
    for (int i = 0; i < 7; ++i) {
        m_weekDays << date.toString(QLatin1String("ddd"));
        date = date.addDays(1);
    }

    m_gridLeftOffset = 0;
    m_gridTopOffset = 0;
    m_mousePressed = false;
    setMouseTracking(true);

    m_showFirstParam = true;
    m_showSecondParam = true;

    m_labelsFont = font();
}

QnScheduleGridWidget::~QnScheduleGridWidget() {
    return;
}

qreal QnScheduleGridWidget::cellSize() const
{
    qreal cellWidth = ((qreal)width() - 0.5 - m_gridLeftOffset)/columnCount();
    qreal cellHeight = ((qreal)height() - 0.5 - m_gridTopOffset)/rowCount();
    return qMin(cellWidth, cellHeight);
}

void QnScheduleGridWidget::initMetrics()
{
    // determine labels font size
    m_labelsFont.setBold(true);
    qreal fontSize = 10.0;
    do
    {
        m_gridLeftOffset = 0;
        m_gridTopOffset = 0;
        m_weekDaysSize.clear();
        m_labelsFont.setPointSizeF(fontSize);
        QFontMetrics metric(m_labelsFont);
        for (int i = 0; i < 7; ++i) {
            QSize sz = metric.size(Qt::TextSingleLine, m_weekDays[i]);
            m_weekDaysSize << sz;
            m_gridLeftOffset = qMax(sz.width(), m_gridLeftOffset);
            m_gridTopOffset = qMax(sz.height(), m_gridTopOffset);
        }
        m_gridLeftOffset += TEXT_SPACING;
        m_gridTopOffset += TEXT_SPACING;
        fontSize += 0.5;
    } while (m_gridLeftOffset < cellSize()*0.5);

    // determine grid font size
    m_gridFont.setPointSizeF(7.0);
    qreal cellSize = this->cellSize();
    while (1)
    {
        QFontMetrics metrics(m_gridFont);
        if (metrics.width(QLatin1String("Md")) > cellSize/2.2)
            break;
        m_gridFont.setPointSizeF(m_gridFont.pointSizeF()+0.5);
    }
}

QSize QnScheduleGridWidget::minimumSizeHint() const
{
    QSize sz;

    qreal fontSize = 14.0;

    QFont font = m_labelsFont;
    font.setBold(true);
    font.setPointSizeF(fontSize);

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
    if (m_weekDaysSize.isEmpty())
        initMetrics();
    qreal cellSize = this->cellSize();

    p.setFont(m_labelsFont);
    for (int y = 0; y < rowCount(); ++y) 
    {
        QColor penClr;
        if (m_mouseMoveCell.x() == -1 && m_mouseMoveCell.y() == y)
            penClr = m_enabled ? SELECTED_LABEL_COLOR : NORMAL_LABEL_COLOR;
        else if (y < 5)
            penClr = NORMAL_LABEL_COLOR;
        else
            penClr = WEEKEND_LABEL_COLOR;
        if (!m_enabled)
            penClr = shiftColor(toGrayscale(penClr), DISABLED_COLOR_SHIFT, DISABLED_COLOR_SHIFT, DISABLED_COLOR_SHIFT);
        p.setPen(penClr);
        p.drawText(QRect(0, cellSize*y+m_gridTopOffset, m_gridLeftOffset-TEXT_SPACING, cellSize), Qt::AlignRight | Qt::AlignVCenter, m_weekDays[y]);
    }
    for (int x = 0; x < columnCount(); ++x) 
    {
        QColor penClr;
        if (m_mouseMoveCell.y() == -1 && m_mouseMoveCell.x() == x)
            penClr = m_enabled ? SELECTED_LABEL_COLOR : NORMAL_LABEL_COLOR;
        else
            penClr = NORMAL_LABEL_COLOR;
        if (!m_enabled)
            penClr = shiftColor(toGrayscale(penClr), DISABLED_COLOR_SHIFT, DISABLED_COLOR_SHIFT, DISABLED_COLOR_SHIFT);
        p.setPen(penClr);
        p.drawText(QRect(m_gridLeftOffset + cellSize*x, 0, cellSize, m_gridTopOffset), Qt::AlignCenter | Qt::AlignVCenter, QString::number(x));
    }


    p.setFont(m_gridFont);
    p.translate(m_gridLeftOffset, m_gridTopOffset);

    // draw grid colors/text
    for (int x = 0; x < columnCount(); ++x)
    {
        for (int y = 0; y < rowCount(); ++y)
        {
            QColor color(m_gridParams[x][y][ColorParam].toUInt());
            QColor colorInside(m_gridParams[x][y][ColorInsideParam].toUInt());
            if (!m_mousePressed) {
                if ((y == m_mouseMoveCell.y()  && x == m_mouseMoveCell.x()) ||
                    (m_mouseMoveCell.y() == -1 && x == m_mouseMoveCell.x()) ||
                    (m_mouseMoveCell.x() == -1 && y == m_mouseMoveCell.y()) ||
                    (m_mouseMoveCell.y() == -1 && m_mouseMoveCell.x() == -1))
                {
                    color = shiftColor(color, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA);
                    colorInside = shiftColor(colorInside, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA);
                }
            }
            if (!m_enabled) {
                color = toGrayscale(color);
                colorInside = toGrayscale(colorInside);
            }

            QPointF leftTop(cellSize*x, cellSize*y);
            QPointF rightBottom(leftTop.x() + cellSize, leftTop.y() + cellSize);
            p.fillRect(QRectF(leftTop.x(), leftTop.y(), cellSize, cellSize), color);

            QColor penClr(255, 255, 255, 128);
            if (!m_enabled)
                penClr = toGrayscale(penClr);
            if (m_showFirstParam && m_showSecondParam)
            {
                QPoint p1 = QPointF(leftTop.x(), rightBottom.y()).toPoint();
                QPoint p2(p1.x() + int(cellSize+0.5), p1.y() - int(cellSize+0.5));
                p.drawLine(p1, p2);
            }

            if (colorInside.toRgb() != color.toRgb()) {
                //p.fillRect(QRectF(leftTop.x() + cellSize/3, leftTop.y() + cellSize/3, cellSize/3, cellSize/3), colorInside);
                p.translate(leftTop);

                p.setBrush(m_enabled ? colorInside : toGrayscale(colorInside));
                QColor penClr(shiftColor(color, -32,-32,-32));
                if (!m_enabled)
                    penClr = toGrayscale(penClr);
                p.setPen(penClr);

                qreal trOffset = cellSize/6;
                qreal trSize = cellSize/10;

                QPointF points[6];
                points[0] = QPointF(cellSize - trOffset - trSize, trOffset);
                points[1] = QPointF(cellSize - trOffset, trOffset);
                points[2] = QPointF(cellSize-trOffset, trOffset + trSize);
                points[3] = QPointF(trOffset + trSize, cellSize - trOffset);
                points[4] = QPointF(trOffset, cellSize - trOffset);
                points[5] = QPointF(trOffset, cellSize - trSize - trOffset);
                p.drawPolygon(points, 6);

                p.translate(-leftTop.x(),-leftTop.y());
            }

            // draw text parameters
            penClr = QColor(255, 255, 255, 128);
            if (!m_enabled)
                penClr = toGrayscale(penClr);

            p.setPen(penClr);
            if (m_showFirstParam && m_showSecondParam)
            {
                p.drawText(QRectF(leftTop, leftTop+QPointF(cellSize/2.0, cellSize/2.0)), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][FirstParam].toString());
                p.drawText(QRectF(leftTop+QPointF(cellSize/2.0, cellSize/2.0), rightBottom), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][SecondParam].toString());
            }
            else if (m_showFirstParam)
                p.drawText(QRectF(leftTop, leftTop+QPointF(cellSize, cellSize)), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][FirstParam].toString());
            else if (m_showSecondParam)
                p.drawText(QRectF(leftTop, leftTop+QPointF(cellSize, cellSize)), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][SecondParam].toString());


            p.setPen(QColor(255, 255, 255));

        }
    }

    // draw grid lines
    QColor penClr(255, 255, 255);
    if (!m_enabled)
        penClr = toGrayscale(penClr);

    p.setPen(penClr);
    for (int x = 0; x <= columnCount(); ++x)
        p.drawLine(QPointF(cellSize*x, 0), QPointF(cellSize * x, cellSize * rowCount()));
    for (int y = 0; y <= rowCount(); ++y)
        p.drawLine(QPointF(0, y*cellSize), QPointF(cellSize * columnCount(), y * cellSize));

    p.translate(-m_gridLeftOffset, -m_gridTopOffset);

    penClr = QColor(255, 255, 255, 128);
    if (!m_enabled)
        penClr = toGrayscale(penClr);

    p.setPen(penClr);
    p.drawLine(QPoint(4, m_gridTopOffset), QPoint(m_gridLeftOffset, m_gridTopOffset));
    p.drawLine(QPoint(m_gridLeftOffset, 4), QPoint(m_gridLeftOffset, m_gridTopOffset));
    if (m_mouseMoveCell.x() == -1 && m_mouseMoveCell.y() == -1)
        p.fillRect(QRectF(QPointF(4,4), QPointF(m_gridLeftOffset-4, m_gridTopOffset-4)), SELECTED_LABEL_COLOR);


    // draw selection
    penClr = subColor(QColor(m_defaultParams[ColorParam].toUInt()), qnGlobals->selectionBorderDelta());
    if (!m_enabled)
        penClr = toGrayscale(penClr);

    p.setPen(penClr);
    QColor brushClr = subColor(QColor(m_defaultParams[ColorParam].toUInt()), qnGlobals->selectionOpacityDelta());
    if (!m_enabled)
        brushClr = toGrayscale(brushClr);

    p.setBrush(brushClr);
    if (!m_selectedRect.isEmpty())
        p.drawRect(m_selectedRect);
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

void QnScheduleGridWidget::setShowFirstParam(bool value)
{
    m_showFirstParam = value;
    update();
}

void QnScheduleGridWidget::setShowSecondParam(bool value)
{
    m_showSecondParam = value;
    update();
}

QVariant QnScheduleGridWidget::cellValue(const QPoint &cell, ParamType paramType) const
{
    if (!isValidCell(cell))
        return QVariant();
    if (paramType < 0 || paramType >= ParamType_Count)
        return QVariant();
    return m_gridParams[cell.x()][cell.y()][paramType];
}

void QnScheduleGridWidget::setCellValue(const QPoint &cell, ParamType paramType, const QVariant &value)
{
    if (!isValidCell(cell))
        return;
    if (paramType < 0 || paramType >= ParamType_Count)
        return;
    
    QVariant &localValue = m_gridParams[cell.x()][cell.y()][paramType];
    if(localValue == value)
        return;

    localValue = value;

    emit cellValueChanged(cell);

    update();
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
    if(qEqual(value, &value[ParamType_Count], localValue))
        return;

    qCopy(value, &value[ParamType_Count], localValue);

    emit cellValueChanged(cell);
}

void QnScheduleGridWidget::setCellValueInternal(const QPoint &cell, ParamType type, const QVariant &value) 
{
    assert(isValidCell(cell));
    assert(type >= 0 && type < ParamType_Count);

    CellParams &localValue = m_gridParams[cell.x()][cell.y()];
    if(localValue[type] == value)
        return;

    localValue[type] = value;

    emit cellValueChanged(cell);
}

void QnScheduleGridWidget::resetCellValues()
{
    CellParams emptyParams;
    emptyParams[FirstParam] = QLatin1String("-");
    emptyParams[SecondParam] = QLatin1String("-");
    emptyParams[ColorInsideParam] = emptyParams[ColorParam] = qnGlobals->noRecordColor().rgba();

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

void QnScheduleGridWidget::setMaxFps(int maxFps)
{
    for (int x = 0; x < columnCount(); ++x)
    {
        for (int y = 0; y < rowCount(); ++y)
        {
            int fps = m_gridParams[x][y][FirstParam].toInt();
            if(fps > maxFps) 
                setCellValueInternal(QPoint(x, y), FirstParam, maxFps);
        }
    }
}

int QnScheduleGridWidget::getMaxFps()
{
    int fps = 0;
    for (int x = 0; x < columnCount(); ++x)
        for (int y = 0; y < rowCount(); ++y) {
            if (m_gridParams[x][y][ColorParam] != qnGlobals->noRecordColor().rgba())
                fps = qMax(fps, m_gridParams[x][y][FirstParam].toInt());
        }
    return fps;
}
