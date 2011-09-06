#include "styledslider.h"

#include <QtGui/QGradient>
#include <QtGui/QPainter>

static const int sliderHeigth = 12;

StyledSlider::StyledSlider(QWidget *parent)
    : QSlider(Qt::Horizontal, parent),
      m_timerId(0)
{
    setMinimumHeight(sliderHeigth + 2);
}

StyledSlider::~StyledSlider()
{
    if (m_timerId)
    {
        killTimer(m_timerId);
        m_timerId = 0;
    }
}

QString StyledSlider::valueText() const
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
    /*QStyleOptionSlider slider;
    initStyleOption(&slider);
    int available = style()->pixelMetric(QStyle::PM_SliderSpaceAvailable, &slider, this);
    const double handlePos = (double)QStyle::sliderPositionFromValue(slider.minimum, slider.maximum, slider.sliderValue, available);*/

    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 0));

    QRect r = contentsRect();
    if (r.height() > sliderHeigth)
    {
        r.moveTop((r.height() - sliderHeigth) / 2);
        r.setHeight(sliderHeigth);
    }
    r.adjust(0, 0, -1, -1);

    const float handlePos = float(value() - minimum()) / (maximum() - minimum());

    QLinearGradient linearGrad(QPointF(0, 0), QPointF(r.width(), 0));
    linearGrad.setColorAt(0, QColor(0, 43, 130));
    linearGrad.setColorAt(handlePos, QColor(186, 239, 255));
    if (!qFuzzyCompare(handlePos, 1.0f))
    {
        linearGrad.setColorAt(handlePos + 0.001, QColor(0, 0, 0, 0));
        linearGrad.setColorAt(1, QColor(0, 0, 0, 0));
    }

    p.setPen(QPen(Qt::darkGray, 1));
    p.setBrush(linearGrad);
    p.drawRect(r);

    if (m_timerId && !m_text.isEmpty())
    {
/*        QRect boundingRect = p.boundingRect(r, Qt::TextDontClip | Qt::AlignCenter, m_text);
        boundingRect.adjust(-3, 1, -2, -1);
        p.setBrush(palette().color(QPalette::Active, QPalette::Highlight));
        p.drawRoundedRect(boundingRect, 2, 2);
*/
        p.setPen(QPen(palette().color(QPalette::Active, QPalette::HighlightedText), 1));
        p.drawText(r, Qt::TextDontClip | Qt::AlignCenter, m_text);
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
