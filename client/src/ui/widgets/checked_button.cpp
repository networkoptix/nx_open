#include "checked_button.h"

#include <QtCore/QEvent>
#include <QtGui/QPainter>

#include "utils/math/color_transformations.h"
#include <utils/math/linear_combination.h>

QPixmap QnCheckedButton::generatePixmap(int size, const QColor &color, const QColor &insideColor, bool hovered, bool checked) 
{
    QPixmap result(size, size);
    result.fill(QColor(Qt::gray));

    QPainter painter(&result);
    painter.setPen(QColor(Qt::black));
    
    QColor brushClr(hovered ? color.lighter() : color);
    if (!isEnabled())
        brushClr = toGrayscale(linearCombine(0.5, brushClr, 0.5, palette().color(QPalette::Disabled, QPalette::Background)));
        //brushClr = shiftColor(brushClr, -64, -64, -64);
    painter.setBrush(brushClr);
    int offset = checked ? 4 : 0;
    painter.drawRect(offset, offset, result.width() - offset * 2, result.height() - offset * 2);

    if (insideColor.toRgb() != color.toRgb()) 
    {
        brushClr = QColor(hovered ? insideColor.lighter() : insideColor);
        if (!isEnabled())
            brushClr = toGrayscale(linearCombine(0.5, brushClr, 0.5, palette().color(QPalette::Disabled, QPalette::Background)));
            //brushClr = shiftColor(brushClr, -64, -64, -64);
        painter.setBrush(brushClr);
        //offset = result.width()/3;
        //painter.drawRect(offset, offset, result.width() - offset * 2, result.height() - offset * 2);
        QPointF points[6];
        qreal cellSize = result.width();
        qreal trOffset = cellSize/6;
        qreal trSize = cellSize/10;
        points[0] = QPointF(cellSize - trOffset - trSize, trOffset);
        points[1] = QPointF(cellSize - trOffset, trOffset);
        points[2] = QPointF(cellSize-trOffset, trOffset + trSize);
        points[3] = QPointF(trOffset + trSize, cellSize - trOffset);
        points[4] = QPointF(trOffset, cellSize - trOffset);
        points[5] = QPointF(trOffset, cellSize - trSize - trOffset);
        painter.drawPolygon(points, 6);
    }


    painter.end();

    return result;
}

QnCheckedButton::QnCheckedButton(QWidget* parent): 
    QToolButton(parent),
    m_insideColorDefined(false)
{
    updateIcon();
}

void QnCheckedButton::updateIcon() {
    QIcon icon;

    const int size = 64;
    QPixmap normal          = generatePixmap(size, m_color,           m_insideColorDefined ? m_insideColor : m_color, false,  false);
    QPixmap normalHovered   = generatePixmap(size, m_color,           m_insideColorDefined ? m_insideColor : m_color, true,   false);
    QPixmap checked         = generatePixmap(size, m_checkedColor,    m_insideColorDefined ? m_insideColor : m_checkedColor, false,  true);
    QPixmap checkedHovered  = generatePixmap(size, m_checkedColor,    m_insideColorDefined ? m_insideColor : m_checkedColor, true,   true);

    icon.addPixmap(normal, QIcon::Normal, QIcon::Off);
    icon.addPixmap(normal, QIcon::Disabled, QIcon::Off);
    icon.addPixmap(normalHovered, QIcon::Active, QIcon::Off);
    icon.addPixmap(normal, QIcon::Selected, QIcon::Off);

    icon.addPixmap(checked, QIcon::Normal, QIcon::On);
    icon.addPixmap(checked, QIcon::Disabled, QIcon::On);
    icon.addPixmap(checkedHovered, QIcon::Active, QIcon::On);
    icon.addPixmap(checked, QIcon::Selected, QIcon::On);

    setIcon(icon);
}

QColor QnCheckedButton::color() const
{
    return m_color;
}

void QnCheckedButton::setColor(const QColor& color)
{
    if(m_color == color)
        return;

    m_color = color;

    updateIcon();
}

void QnCheckedButton::setInsideColor(const QColor& color)
{
    if(m_insideColor == color)
        return;

    m_insideColor = color;
    m_insideColorDefined = true;

    updateIcon();
}

QColor QnCheckedButton::checkedColor() const
{
    return m_checkedColor;
}

void QnCheckedButton::setCheckedColor(const QColor& color)
{
    if(m_checkedColor == color)
        return;

    m_checkedColor = color;

    updateIcon();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnCheckedButton::event(QEvent *event) {
    if(event->type() == QEvent::EnabledChange)
        updateIcon();

    return base_type::event(event);
}

