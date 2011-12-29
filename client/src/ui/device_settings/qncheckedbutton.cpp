#include "qncheckedbutton.h"

QnCheckedButton::QnCheckedButton(QWidget* parent): QToolButton(parent)
{
    m_checkonLastPaint = -1;
}

void QnCheckedButton::checkStateSet()
{
    QPixmap image(width(),height());
    QPainter p (&image);
    p.setPen(QColor(0,0,0));
    if (isChecked())
        p.setBrush(m_checkedColor);
    else
        p.setBrush(m_color);
    int Offs = isChecked() ? 4 : 0;
    p.drawRect(Offs, Offs, image.width() - Offs*2, image.height() - Offs*2);
    QIcon icon;

    icon.addPixmap(image, QIcon::Normal, QIcon::Off);
    icon.addPixmap(image, QIcon::Normal, QIcon::On);

    icon.addPixmap(image, QIcon::Disabled, QIcon::Off);
    icon.addPixmap(image, QIcon::Disabled, QIcon::On);

    icon.addPixmap(image, QIcon::Active, QIcon::Off);
    icon.addPixmap(image, QIcon::Active, QIcon::On);

    icon.addPixmap(image, QIcon::Selected, QIcon::Off);
    icon.addPixmap(image, QIcon::Selected, QIcon::On);

    setIcon(icon);
}

void QnCheckedButton::paintEvent(QPaintEvent * event)
{
    if (m_checkonLastPaint != (int) isChecked())
    {
        checkStateSet();
        m_checkonLastPaint = (int) isChecked();
    }
    QToolButton::paintEvent(event);
}

QColor QnCheckedButton::color() const
{
    return m_color;
}

void QnCheckedButton::setColor(const QColor& color)
{
    m_color = color;
}

void QnCheckedButton::setCheckedColor(const QColor& color)
{
    m_checkedColor = color;
}
