#include "generic_palette.h"

#include <QtCore/QtMath>

const int QnPaletteColor::kMaxAlpha = 255;

uint qHash(const QColor &color)
{
    return qHash(color.name());
}

QnColorList::QnColorList():
    base_type(),
    m_core(),
    m_contrast()
{
}

void QnColorList::setCoreColor(const QColor& color)
{
    m_core = color;
}

QColor QnColorList::coreColor() const
{
    return m_core;
}

void QnColorList::setContrastColor(const QColor& color)
{
    m_contrast = color;
}

QColor QnColorList::contrastColor() const
{
    return m_contrast;
}


QnPaletteColor::QnPaletteColor(QColor fallbackColor)
    : m_index(-1)
    , m_alpha(kMaxAlpha)
    , m_fallbackColor(fallbackColor)
{
}

QnPaletteColor::QnPaletteColor(const QString &group,
        const int index,
        const QnColorList &palette, const int alpha)
    : m_group(group)
    , m_index(index)
    , m_palette(palette)
    , m_alpha(alpha)
{
}

QnPaletteColor::operator QColor() const
{
    return color();
}

QString QnPaletteColor::group() const
{
    return m_group;
}

int QnPaletteColor::index() const
{
    return m_index;
}

QColor QnPaletteColor::color() const
{
    if (!isValid())
        return m_fallbackColor;

    QColor result = m_palette[m_index];
    result.setAlpha(m_alpha);
    return result;
}

int QnPaletteColor::alpha() const
{
    return m_alpha;
}

void QnPaletteColor::setAlpha(int alpha)
{
    m_alpha = alpha;
}

void QnPaletteColor::setAlphaF(qreal alpha)
{
    setAlpha(kMaxAlpha * alpha);
}

void QnPaletteColor::setFallbackColor(QColor fallbackColor)
{
    m_fallbackColor = fallbackColor;
}

bool QnPaletteColor::isValid() const
{
    return m_index >= 0;
}

QnPaletteColor QnPaletteColor::darker(int shift) const
{
    if (isValid())
        return QnPaletteColor(m_group, qBound(0, m_index - shift, m_palette.size() - 1), m_palette, m_alpha);
    /*
    Multiplicative coefficient for scaling fallback color brightness when shift == 1
    Full coefficient is qPow(kFallbackScaleFactor, shift)
    */
    static const qreal kFallbackScaleFactor = 1.04;
    return QnPaletteColor(m_fallbackColor.darker(qRound(100.0 * qPow(kFallbackScaleFactor, shift))));
}

QnPaletteColor QnPaletteColor::lighter(int shift) const
{
    return darker(-shift);
}

QnPaletteColor &QnPaletteColor::operator =(const QnPaletteColor &other)
{
    m_group = other.m_group;
    m_index = other.m_index;
    m_palette = other.m_palette;
    m_alpha = other.m_alpha;
    return *this;
}


QnGenericPalette::QnGenericPalette()
{
}

QnColorList QnGenericPalette::colors(const QString &group) const
{
    return m_colorsByGroup.value(group);
}

void QnGenericPalette::setColors(const QString &group, const QnColorList &colors)
{
    removeColors(group);
    addColors(group, colors);
}

QnPaletteColor QnGenericPalette::color(const QColor &color) const
{
    QColor searchColor = color;
    searchColor.setAlpha(QnPaletteColor::kMaxAlpha);

    QnPaletteColor result = m_colorByQColor.value(searchColor);
    result.setFallbackColor(color);

    result.setAlpha(color.alpha());
    return result;
}

QnPaletteColor QnGenericPalette::color(const QString &group, int index, int alpha) const
{
    QnColorList colors = m_colorsByGroup.value(group);
    if (index >= colors.size())
        return QnPaletteColor();

    return QnPaletteColor(group, index, colors, alpha);
}

void QnGenericPalette::removeColors(const QString &group)
{
    for (const QColor &color: m_colorsByGroup.take(group))
        m_colorByQColor.remove(color);
}

void QnGenericPalette::addColors(const QString &group, const QnColorList &colors)
{
    m_colorsByGroup[group] = colors;

    for (int index = 0; index < colors.size(); ++index)
        m_colorByQColor.insert(colors[index], QnPaletteColor(group, index, colors));
}
