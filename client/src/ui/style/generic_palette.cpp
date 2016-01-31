#include "generic_palette.h"

uint qHash(const QColor &color)
{
    return qHash(color.name());
}

QnPaletteColor::QnPaletteColor()
    : m_index(-1)
{
}

QnPaletteColor::QnPaletteColor(
        const QString &group,
        const int index,
        const QnColorList &palette)
    : m_group(group)
    , m_index(index)
    , m_palette(palette)
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
        return QColor();

    return m_palette[m_index];
}

bool QnPaletteColor::isValid() const
{
    return m_index >= 0;
}

QnPaletteColor QnPaletteColor::darker(int shift) const
{
    if (!isValid())
        return QnPaletteColor();

    return QnPaletteColor(m_group, qBound(0, m_index - shift, m_palette.size() - 1), m_palette);
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
    return m_colorByQColor.value(color);
}

QnPaletteColor QnGenericPalette::color(const QString &group, int index) const
{
    QnColorList colors = m_colorsByGroup.value(group);
    if (index >= colors.size())
        return QnPaletteColor();

    return QnPaletteColor(group, index, colors);
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
