#include "styledslider.h"

#include <QtGui/QGradient>
#include <QtGui/QPainter>

StyledSlider::StyledSlider(QWidget *parent)
    : QSlider(Qt::Horizontal, parent),
      m_timerId(0)
{
}

StyledSlider::StyledSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent),
      m_timerId(0)
{
}

StyledSlider::~StyledSlider()
{
    if (m_timerId)
    {
        killTimer(m_timerId);
        m_timerId = 0;
    }
}

QString StyledSlider::valueTex() const
{
    return m_text;
}

void StyledSlider::setValueText(const QString &text)
{
    if (m_text == text)
        return;

    m_text = text;
    update();
}

void StyledSlider::paintEvent(QPaintEvent *)
{
    static const int gradHeigth = 10;

    /*QStyleOptionSlider slider;
    initStyleOption(&slider);
    int available = style()->pixelMetric(QStyle::PM_SliderSpaceAvailable, &slider, this);
    const double handlePos = (double)QStyle::sliderPositionFromValue(slider.minimum, slider.maximum, slider.sliderValue, available);*/

    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 0));

    QRect r = contentsRect();
    const double handlePos = (double)r.width() * (value() - minimum()) / (maximum() - minimum());

    p.setPen(QPen(Qt::darkGray, 1));
    p.drawRect(QRect(r.x(), (height() - gradHeigth) / 2 - 1, r.width() - 1, gradHeigth + 2));

    QLinearGradient linearGrad(QPointF(0, 0), QPointF(handlePos, height()));
    linearGrad.setColorAt(0, QColor(0, 43, 130));
    linearGrad.setColorAt(1, QColor(186, 239, 255));
    p.fillRect(QRect(r.x() + 1, (height() - gradHeigth) / 2, handlePos, gradHeigth), linearGrad);

    if (m_timerId && !m_text.isEmpty())
    {
        p.setPen(QPen(palette().color(QPalette::Active, QPalette::HighlightedText), 1));
        p.drawText(r, Qt::AlignCenter, m_text);
    }
}

void StyledSlider::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId)
    {
        killTimer(m_timerId);
        m_timerId = 0;
        update();
    }

    QSlider::timerEvent(event);
}

void StyledSlider::sliderChange(SliderChange change)
{
    if (m_timerId)
        killTimer(m_timerId);
    m_timerId = startTimer(2500);

    m_text = QString::number(value());

    QSlider::sliderChange(change);
}
