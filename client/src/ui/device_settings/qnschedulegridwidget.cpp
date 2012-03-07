#include "qnschedulegridwidget.h"

#include <QtGui/QApplication>

#include "ui/style/globals.h"
#include "settings.h"

static const QColor NORMAL_LABEL_COLOR(255,255,255);
static const QColor WEEKEND_LABEL_COLOR(255,128,128);
static const QColor SELECTED_LABEL_COLOR(64,128, 192);
//static const QColor DISABLED_LABEL_COLOR(64,64,64);
static const int DISABLED_COLOR_SHIFT = -72;

QnScheduleGridWidget::QnScheduleGridWidget(QWidget *parent)
    : QWidget(parent)
{
    m_enabled = true;
    m_defaultParams[FirstParam] = 10;
    m_defaultParams[SecondParam] = QLatin1String("Lo");
    m_defaultParams[ColorParam] = QColor(COLOR_LIGHT,0, 0).rgba();

    for (int col = 0; col < columnCount(); ++col) {
        for (int row = 0; row < rowCount(); ++row)
            qCopy(m_defaultParams, &m_defaultParams[ParamType_Count], m_gridParams[col][row]);
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

    m_showFirstParam = true;
    m_showSecondParam = true;

    m_labelsFont = font();
}

qreal QnScheduleGridWidget::getCellSize() const
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
    } while (m_gridLeftOffset < getCellSize()*0.5);

    // determine grid font size
    m_gridFont.setPointSizeF(7.0);
    qreal cellSize = getCellSize();
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

void toGrayColor(QColor& color)
{
    float Y  = (0.257 * color.red()) + (0.504 * color.green()) + (0.098 * color.blue()) + 16;

    int v = qMax(0, qMin(255, int (1.164*(Y - 32))));
    color.setBlue(v);
    color.setGreen(v);
    color.setRed(v);
}

void shiftColor(QColor& color, int deltaR, int deltaG, int deltaB)
{
    color = QColor(qMax(0,qMin(255,color.red()+deltaR)), qMax(0,qMin(255,color.green()+deltaG)), qMax(0,qMin(255,color.blue()+deltaR)), color.alpha());
}

void QnScheduleGridWidget::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    if (m_weekDaysSize.isEmpty())
        initMetrics();
    qreal cellSize = getCellSize();

    p.setFont(m_labelsFont);
    for (int y = 0; y < rowCount(); ++y) 
    {
        QColor penClr;
        if (m_mouseMoveCell.x() == -1 && m_mouseMoveCell.y() == y)
            penClr = SELECTED_LABEL_COLOR;
        else if (y < 5)
            penClr = NORMAL_LABEL_COLOR;
        else
            penClr = WEEKEND_LABEL_COLOR;
        if (!m_enabled) {
            toGrayColor(penClr);
            shiftColor(penClr, DISABLED_COLOR_SHIFT, DISABLED_COLOR_SHIFT, DISABLED_COLOR_SHIFT);
        }
        p.setPen(penClr);
        p.drawText(QRect(0, cellSize*y+m_gridTopOffset, m_gridLeftOffset-TEXT_SPACING, cellSize), Qt::AlignRight | Qt::AlignVCenter, m_weekDays[y]);
    }
    for (int x = 0; x < columnCount(); ++x) 
    {
        QColor penClr;
        if (m_mouseMoveCell.y() == -1 && m_mouseMoveCell.x() == x)
            penClr = SELECTED_LABEL_COLOR;
        else
            penClr = NORMAL_LABEL_COLOR;
        if (!m_enabled) {
            toGrayColor(penClr);
            shiftColor(penClr, DISABLED_COLOR_SHIFT, DISABLED_COLOR_SHIFT, DISABLED_COLOR_SHIFT);
        }
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
            if (!m_mousePressed) {
                if (y == m_mouseMoveCell.y() && x == m_mouseMoveCell.x() ||
                    m_mouseMoveCell.y() == -1 && x == m_mouseMoveCell.x() ||
                    m_mouseMoveCell.x() == -1 && y == m_mouseMoveCell.y() ||
                    m_mouseMoveCell.y() == -1 && m_mouseMoveCell.x() == -1)
                {
                    shiftColor(color, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA);
                }
            }
            if (!m_enabled)
                toGrayColor(color);


            QPointF leftTop(cellSize*x, cellSize*y);
            QPointF rightBottom(leftTop.x() + cellSize, leftTop.y() + cellSize);
            //p.setBrush(QBrush(color));
            //p.drawRect(leftTop.x(), leftTop.y(), cellSize, cellSize);
            p.fillRect(QRectF(leftTop.x(), leftTop.y(), cellSize, cellSize), color);

            // draw text parameters
            QColor penClr(255,255,255, 128);
            if (!m_enabled)
                toGrayColor(penClr);

            p.setPen(penClr);
            if (m_showFirstParam && m_showSecondParam)
            {
                QPoint p1 = QPointF(leftTop.x(), rightBottom.y()).toPoint();
                QPoint p2(p1.x() + int(cellSize+0.5), p1.y() - int(cellSize+0.5));
                p.drawLine(p1, p2);
                p.drawText(QRectF(leftTop, leftTop+QPointF(cellSize/2.0, cellSize/2.0)), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][FirstParam].toString());
                p.drawText(QRectF(leftTop+QPointF(cellSize/2.0, cellSize/2.0), rightBottom), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][SecondParam].toString());
            }
            else if (m_showFirstParam)
                p.drawText(QRectF(leftTop, leftTop+QPointF(cellSize, cellSize)), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][FirstParam].toString());
            else if (m_showSecondParam)
                p.drawText(QRectF(leftTop, leftTop+QPointF(cellSize, cellSize)), Qt::AlignCenter | Qt::AlignHCenter, m_gridParams[x][y][SecondParam].toString());
            p.setPen(QColor(255,255,255));

        }
    }

    // draw grid lines
    QColor penClr(255, 255, 255);
    if (!m_enabled)
        toGrayColor(penClr);

    p.setPen(penClr);
    for (int x = 0; x <= columnCount(); ++x)
        p.drawLine(QPointF(cellSize*x, 0), QPointF(cellSize * x, cellSize * rowCount()));
    for (int y = 0; y <= rowCount(); ++y)
        p.drawLine(QPointF(0, y*cellSize), QPointF(cellSize * columnCount(), y * cellSize));

    p.translate(-m_gridLeftOffset, -m_gridTopOffset);

    penClr = QColor(255,255,255, 128);
    if (!m_enabled)
        toGrayColor(penClr);

    p.setPen(penClr);
    p.drawLine(QPoint(4, m_gridTopOffset), QPoint(m_gridLeftOffset, m_gridTopOffset));
    p.drawLine(QPoint(m_gridLeftOffset, 4), QPoint(m_gridLeftOffset, m_gridTopOffset));
    if (m_mouseMoveCell.x() == -1 && m_mouseMoveCell.y() == -1)
        p.fillRect(QRectF(QPointF(4,4), QPointF(m_gridLeftOffset-4, m_gridTopOffset-4)), SELECTED_LABEL_COLOR);


    // draw selection
    penClr = QColor(qnGlobals->motionRubberBandBorderColor());
    if (!m_enabled)
        toGrayColor(penClr);

    p.setPen(penClr);
    QColor brushClr(qnGlobals->motionRubberBandColor());
    if (!m_enabled)
        toGrayColor(brushClr);

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
    QPoint cell = getCell(event->pos(), false);
    if (cell != m_mouseMoveCell)
    {
        m_mouseMoveCell = cell;
        update();
    }
}

QPoint QnScheduleGridWidget::getCell(const QPoint &p, bool doTruncate) const
{
    qreal cellSize = getCellSize();
    int cellX = (p.x() - m_gridLeftOffset)/cellSize;
    int cellY = (p.y() - m_gridTopOffset)/cellSize;

    if (doTruncate)
        return QPoint(qBound(0, columnCount() - 1, cellX), qBound(0, rowCount() - 1, cellY));

    if (p.x() < m_gridLeftOffset)
        cellX = -1;
    if (p.y() < m_gridTopOffset)
        cellY = -1;

    if (cellX < columnCount() && cellY < rowCount())
        return QPoint(cellX, cellY);

    return QPoint(-2, -2);
}

void QnScheduleGridWidget::mousePressEvent(QMouseEvent *event)
{
    QPoint cell = getCell(event->pos(), false);
    if (event->modifiers() & Qt::AltModifier)
    {
        if (cell.x() >= 0 && cell.y() >= 0)
            emit needReadCellParams(cell);
        return;
    }

    m_mousePressPos = event->pos();
    m_mousePressed = true;
    if (cell.x() == -1 && cell.y() == -1)
    {
        for (int y = 0; y < rowCount(); ++y) {
            for (int x = 0; x < columnCount(); ++x)
                updateCellValues(QPoint(x, y));
        }
    }
    else if (cell.x() == -1) {
        for (int x = 0; x < columnCount(); ++x)
            updateCellValues(QPoint(x, cell.y()));
    }
    else if (cell.y() == -1) {
        for (int y = 0; y < rowCount(); ++y)
            updateCellValues(QPoint(cell.x(), y));
    }
    else {
        updateCellValues(cell);
    }
    update();
    setEnabled(true);
}

void QnScheduleGridWidget::updateCellValues(const QPoint& cell)
{
    if (cell.x() < 0 || cell.y() < 0 || cell.x() >= columnCount() || cell.y() >= rowCount())
        return;

    qCopy(m_defaultParams, &m_defaultParams[ParamType_Count], m_gridParams[cell.x()][cell.y()]);
    update();
}

void QnScheduleGridWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_mousePressed)
    {
        QPoint cell1 = getCell(m_mousePressPos, true);
        QPoint cell2 = getCell(event->pos(), true);
        QRect r = QRect(cell1, cell2).normalized();
        for (int x = r.left(); x <= r.right(); ++x)
        {
            for (int y = r.top(); y <= r.bottom(); ++y)
                updateCellValues(QPoint(x,y));
        }
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

QVariant QnScheduleGridWidget::getCellValue(const QPoint &cell, ParamType paramType) const
{
    if (cell.x() < 0 || cell.y() < 0 || cell.x() >= columnCount() || cell.y() >= rowCount())
        return QVariant();
    if (paramType < 0 || paramType >= ParamType_Count)
        return QVariant();
    return m_gridParams[cell.x()][cell.y()][paramType];
}

void QnScheduleGridWidget::setCellValue(const QPoint &cell, ParamType paramType, const QVariant &value)
{
    if (cell.x() < 0 || cell.y() < 0 || cell.x() >= columnCount() || cell.y() >= rowCount())
        return;
    if (paramType < 0 || paramType >= ParamType_Count)
        return;
    m_gridParams[cell.x()][cell.y()][paramType] = value;
    update();
}

void QnScheduleGridWidget::resetCellValues(const QPoint& cell)
{
    if (cell.x() < 0 || cell.y() < 0 || cell.x() >= columnCount() || cell.y() >= rowCount())
        return;

    QVariant *p = m_gridParams[cell.x()][cell.y()];
    QVariant *e = p + ParamType_Count;
    while (p != e)
        *p++ = QVariant();
    update();
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
