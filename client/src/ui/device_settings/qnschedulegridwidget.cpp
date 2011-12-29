#include "qnschedulegridwidget.h"
#include "ui/graphics/instruments/motionselectioninstrument.h"

static const int TEXT_SPACING = 4;
static const QColor NORMAL_LABEL_COLOR(255,255,255);
static const QColor WEEKEND_LABEL_COLOR(255,128,128);
static const QColor SELECTED_LABEL_COLOR(64,128, 192);

QnScheduleGridWidget::QnScheduleGridWidget(QWidget* parent):
    QWidget(parent)
{
    for (int x = 0; x < COL_COUNT; ++x)
    {
        for (int y = 0; y < ROW_COUNT; ++y)
        {
            m_gridParams[x][y][ParamType_Color] = QColor(COLOR_LIGHT,0,0).rgba();
            m_gridParams[x][y][ParamType_First] = 25;
            m_gridParams[x][y][ParamType_Second] = "Hi";
        }
    }
    QDate date(2010,1,1);
    date = date.addDays(1 - date.dayOfWeek());
    for (int i = 0; i < 7; ++i) {
        m_weekDays << date.toString("ddd");
        date = date.addDays(1);
    }

    m_gridLeftOffset = 0;
    m_gridTopOffset = 0;
    m_mousePressed = false;
    setMouseTracking(true);

    m_defaultParams[ParamType_Color] = QColor(0, COLOR_LIGHT,0).rgba();
    m_defaultParams[ParamType_First] = 3;
    m_defaultParams[ParamType_Second] = "Lo";
    m_showFirstParam = true;
    m_showSecondParam = true;

}

qreal QnScheduleGridWidget::getCellSize()
{
    qreal cellWidth = (width() - 0.5 - m_gridLeftOffset)/(qreal)COL_COUNT;
    qreal cellHeight = (height() - 0.5 - m_gridTopOffset)/(qreal)ROW_COUNT;
    return qMin(cellWidth, cellHeight);
}

void QnScheduleGridWidget::initMetrics()
{
    // determine labels font size
    m_labelsFont.setBold(true);
    qreal fontSize = 7.0;
    do
    {
        m_gridLeftOffset = 0;
        m_weekDaysSize.clear();
        m_labelsFont.setPointSizeF(fontSize);
        QFontMetrics metric(m_labelsFont);
        for (int i = 0; i < 7; ++i) {
            m_weekDaysSize << metric.size(Qt::TextSingleLine, m_weekDays[i]);
            m_gridLeftOffset = qMax(m_gridLeftOffset, m_weekDaysSize.last().width());
        }
        m_gridTopOffset = metric.size(Qt::TextSingleLine,"23").height();
        m_gridLeftOffset += TEXT_SPACING;
        m_gridTopOffset += TEXT_SPACING;
        fontSize += 0.5;
    } while (m_gridLeftOffset < getCellSize()*0.5);

    // determine grid font size
    m_gridFont.setPointSizeF(7.0);
    qreal cellSize = getCellSize();
    while (1)
    {
        QFontMetrics metrics(m_gridFont);
        if (metrics.width("Md") > cellSize/2.2)
            break;
        m_gridFont.setPointSizeF(m_gridFont.pointSizeF()+0.5);
    }
}

void QnScheduleGridWidget::paintEvent(QPaintEvent * event)
{
    QPainter p(this);
    if (m_weekDaysSize.isEmpty())
        initMetrics();
    qreal cellSize = getCellSize();

    p.setFont(m_labelsFont);
    for (int y = 0; y < ROW_COUNT; ++y) 
    {
        if (m_mouseMoveCell.x() == -1 && m_mouseMoveCell.y() == y)
            p.setPen(SELECTED_LABEL_COLOR);
        else if (y < 5)
            p.setPen(NORMAL_LABEL_COLOR);
        else
            p.setPen(WEEKEND_LABEL_COLOR);
        p.drawText(QRect(0, cellSize*y+m_gridTopOffset, m_gridLeftOffset-TEXT_SPACING, cellSize), Qt::AlignRight | Qt::AlignVCenter, m_weekDays[y]);
    }
    for (int x = 0; x < COL_COUNT; ++x) {
        if (m_mouseMoveCell.y() == -1 && m_mouseMoveCell.x() == x)
            p.setPen(SELECTED_LABEL_COLOR);
        else
            p.setPen(NORMAL_LABEL_COLOR);
        p.drawText(QRect(m_gridLeftOffset + cellSize*x, 0, cellSize, m_gridTopOffset), Qt::AlignCenter | Qt::AlignVCenter, QString::number(x));
    }


    p.setFont(m_gridFont);
    p.translate(m_gridLeftOffset, m_gridTopOffset);

    // draw grid colors/text
    for (int x = 0; x < COL_COUNT; ++x)
    {
        for (int y = 0; y < ROW_COUNT; ++y)
        {
            QColor color(m_gridParams[x][y][ParamType_Color].toUInt());
            if (!m_mousePressed) {
                if (y == m_mouseMoveCell.y() && x == m_mouseMoveCell.x() || 
                    m_mouseMoveCell.y() == -1 && x == m_mouseMoveCell.x() || 
                    m_mouseMoveCell.x() == -1 && y == m_mouseMoveCell.y() ||
                    m_mouseMoveCell.y() == -1 && m_mouseMoveCell.x() == -1)
                {
                    color = QColor(qMin(255,color.red()+SEL_CELL_CLR_DELTA), qMin(255,color.green()+SEL_CELL_CLR_DELTA), qMin(255,color.blue()+SEL_CELL_CLR_DELTA));
                }
            }

            QPointF leftTop(cellSize*x, cellSize*y);
            QPointF rightBottom(leftTop.x() + cellSize, leftTop.y() + cellSize);
            //p.setBrush(QBrush(color));
            //p.drawRect(leftTop.x(), leftTop.y(), cellSize, cellSize);
            p.fillRect(QRectF(leftTop.x(), leftTop.y(), cellSize, cellSize), color);

            // draw text parameters
            p.setPen(QColor(255,255,255, 128));
            if (m_showFirstParam && m_showSecondParam)
            {
                QPoint p1 = QPointF(leftTop.x(), rightBottom.y()).toPoint();
                QPoint p2(p1.x() + int(cellSize+0.5), p1.y() - int(cellSize+0.5));
                p.drawLine(p1, p2);
                p.drawText(QRectF(leftTop, leftTop+QPointF(cellSize/2.0, cellSize/2.0)), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][ParamType_First].toString());
                p.drawText(QRectF(leftTop+QPointF(cellSize/2.0, cellSize/2.0), rightBottom), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][ParamType_Second].toString());
            }
            else if (m_showFirstParam)
                p.drawText(QRectF(leftTop, leftTop+QPointF(cellSize, cellSize)), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][ParamType_First].toString());
            else if (m_showSecondParam)
                p.drawText(QRectF(leftTop, leftTop+QPointF(cellSize, cellSize)), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][ParamType_Second].toString());
            p.setPen(QColor(255,255,255));

        }
    }

    // draw grid lines
    p.setPen(QColor(255,255,255));
    for (int x = 0; x <= COL_COUNT; ++x)
        p.drawLine(QPointF(cellSize*x, 0), QPointF(cellSize*x, cellSize*ROW_COUNT));
    for (int y = 0; y <= ROW_COUNT; ++y)
        p.drawLine(QPointF(0, y*cellSize), QPointF(cellSize*COL_COUNT, y*cellSize));

    p.translate(-m_gridLeftOffset, -m_gridTopOffset);

    p.setPen(QColor(255,255,255, 128));
    p.drawLine(QPoint(4, m_gridTopOffset), QPoint(m_gridLeftOffset, m_gridTopOffset));
    p.drawLine(QPoint(m_gridLeftOffset, 4), QPoint(m_gridLeftOffset, m_gridTopOffset));
    if (m_mouseMoveCell.x() == -1 && m_mouseMoveCell.y() == -1)
        p.fillRect(QRectF(QPointF(4,4), QPointF(m_gridLeftOffset-4, m_gridTopOffset-4)), SELECTED_LABEL_COLOR);


    // draw selection
    p.setPen(SELECT_ARIA_PEN_COLOR);
    p.setBrush(SELECT_ARIA_BRUSH_COLOR);
    if (!m_selectedRect.isEmpty())
        p.drawRect(m_selectedRect);
}


void QnScheduleGridWidget::leaveEvent(QEvent * event )
{
    m_mouseMovePos = QPoint(-1,-1);
    m_mouseMoveCell = QPoint(-2, -2);
    update();
}

void QnScheduleGridWidget::mouseMoveEvent(QMouseEvent * event )
{
    m_mouseMovePos = event->pos();
    if (m_mousePressed && (event->pos() - m_mousePressPos).manhattanLength() >= QApplication::startDragDistance())
    {
        m_selectedRect = QRect(m_mousePressPos, event->pos()).normalized();
        update();
    }
    QPoint cell = getCell(event->pos(), false);
    if (cell != m_mouseMoveCell)
    {
        m_mouseMoveCell = cell;
        update();
    }
}

QPoint QnScheduleGridWidget::getCell(const QPoint& p, bool doTruncate)
{
    qreal cellSize = getCellSize();
    int cellX = (p.x() - m_gridLeftOffset)/cellSize;
    int cellY = (p.y() - m_gridTopOffset)/cellSize;
    
    if (doTruncate)
        return QPoint(qBound(0, COL_COUNT-1, cellX), qBound(0, ROW_COUNT-1, cellY));

    if (p.x() < m_gridLeftOffset)
        cellX = -1;
    if (p.y() < m_gridTopOffset)
        cellY = -1;

    if (cellX < COL_COUNT && cellY < ROW_COUNT)
        return QPoint(cellX, cellY);
    else
        return QPoint(-2, -2);
}

void QnScheduleGridWidget::mousePressEvent(QMouseEvent * event )
{
    m_mousePressPos = event->pos();
    m_mousePressed = true;
    QPoint cell = getCell(event->pos(), false);
    
    if (cell.x() == -1 && cell.y() == -1) 
    {
        for (int y = 0; y < ROW_COUNT; ++y)
            for (int x = 0; x < COL_COUNT; ++x)
                updateCellValue(QPoint(x, y));
    }
    else if (cell.x() == -1) {
        for (int x = 0; x < COL_COUNT; ++x)
            updateCellValue(QPoint(x, cell.y()));
    }
    else if (cell.y() == -1) {
        for (int y = 0; y < ROW_COUNT; ++y)
            updateCellValue(QPoint(cell.x(), y));
    }
    else {
        updateCellValue(cell);
    }
    update();
}

void QnScheduleGridWidget::updateCellValue(const QPoint& cell)
{
    if (cell.x() >= 0 && cell.y() >= 0)
    {
        qCopy(m_defaultParams, &m_defaultParams[ParamNum_Dummy], m_gridParams[cell.x()][cell.y()]);
        update();
    }
}

void QnScheduleGridWidget::mouseReleaseEvent(QMouseEvent * event)
{
    QPoint cell1 = getCell(m_mousePressPos, true);
    QPoint cell2 = getCell(event->pos(), true);
    QRect r = QRect(cell1, cell2).normalized();
    for (int x = r.left(); x <= r.right(); ++x)
    {
        for (int y = r.top(); y <= r.bottom(); ++y)
            updateCellValue(QPoint(x,y));
    }

    m_mousePressed = false;
    m_selectedRect = QRect();
    update();
}

void QnScheduleGridWidget::setDefaultParam(ParamType number, const QVariant& value)
{
    m_defaultParams[(int)number] = value;
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

void QnScheduleGridWidget::resizeEvent(QResizeEvent * event)
{
    initMetrics();
}
