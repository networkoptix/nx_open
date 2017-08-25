#include "aspect_ratio.h"

#include <nx/utils/log/assert.h>

namespace
{
    const QList<QnAspectRatio> standardRatios =
            QList<QnAspectRatio>()
                << QnAspectRatio(4, 3)
                << QnAspectRatio(16, 9)
                << QnAspectRatio(1, 1)
                << QnAspectRatio(3, 4)
                << QnAspectRatio(9, 16);
}

QnAspectRatio::QnAspectRatio()
{
}

QnAspectRatio::QnAspectRatio(int width, int height):
    m_width(width),
    m_height(height)
{
}

QnAspectRatio::QnAspectRatio(const QSize& size):
    m_width(size.width()),
    m_height(size.height())
{
}

bool QnAspectRatio::isValid() const
{
    return m_width > 0 && m_height > 0;
}

int QnAspectRatio::width() const
{
    return m_width;
}

int QnAspectRatio::height() const
{
    return m_height;
}

float QnAspectRatio::toFloat() const
{
    NX_ASSERT(isValid());
    if (!isValid())
        return kInvalidAspectRatio;
    return static_cast<float>(m_width) / m_height;
}

QString QnAspectRatio::toString() const
{
    return toString(lit("%1:%2"));
}

QString QnAspectRatio::toString(const QString &format) const
{
    return format.arg(m_width).arg(m_height);
}

QList<QnAspectRatio> QnAspectRatio::standardRatios()
{
    return ::standardRatios;
}

QnAspectRatio QnAspectRatio::closestStandardRatio(float aspectRatio)
{
    QnAspectRatio closest = ::standardRatios.first();
    qreal diff = qAbs(aspectRatio - ::standardRatios.first().toFloat());

    foreach (const QnAspectRatio &ratio, ::standardRatios)
    {
        qreal d = qAbs(aspectRatio - ratio.toFloat());
        if (d < diff)
        {
            diff = d;
            closest = ratio;
        }
    }

    return closest;
}

bool QnAspectRatio::isRotated90(qreal angle)
{
    while (angle > 180)
        angle -= 180;
    return qAbs(90 - qAbs(angle)) < 45;
}

bool QnAspectRatio::operator ==(const QnAspectRatio &other) const
{
    return m_width == other.m_width && m_height == other.m_height;
}
