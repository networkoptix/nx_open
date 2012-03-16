#include "qncheckedbutton.h"

#include <QtGui/QPainter>

namespace {
    QPixmap generatePixmap(int size, const QColor &color, bool hovered, bool checked) {
        QPixmap result(size, size);
        result.fill(QColor(Qt::gray));

        QPainter painter(&result);
        painter.setPen(QColor(Qt::black));
        painter.setBrush(hovered ? color.lighter() : color);
        int offset = checked ? 4 : 0;
        painter.drawRect(offset, offset, result.width() - offset * 2, result.height() - offset * 2);
        painter.end();

        return result;
    }

} // anonymous namespace

QnCheckedButton::QnCheckedButton(QWidget* parent): QToolButton(parent)
{
    updateIcon();
}

void QnCheckedButton::updateIcon() {
    QIcon icon;

    const int size = 64;

    QPixmap normal          = generatePixmap(64, m_color,           false,  false);
    QPixmap normalHovered   = generatePixmap(64, m_color,           true,   false);
    QPixmap checked         = generatePixmap(64, m_checkedColor,    false,  true);
    QPixmap checkedHovered  = generatePixmap(64, m_checkedColor,    true,   true);

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
