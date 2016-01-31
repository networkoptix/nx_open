#pragma once

#include <QtCore/QHash>
#include <QtGui/QColor>

typedef QList<QColor> QnColorList;

class QnGenericPalette;

class QnPaletteColor
{
public:
    QnPaletteColor();

    QnPaletteColor(const QString &group,
                   const int index,
                   const QnColorList &palette);

    QString group() const;
    int index() const;
    QColor color() const;

    bool isValid() const;

    QnPaletteColor darker(int shift) const;
    QnPaletteColor lighter(int shift) const;

    operator QColor() const;

    QnPaletteColor &operator =(const QnPaletteColor &other);

private:
    QString m_group;
    int m_index;
    QnColorList m_palette;
};

class QnGenericPalette
{
public:
    QnGenericPalette();

    QnColorList colors(const QString &group) const;
    void setColors(const QString &group, const QnColorList &colors);

    QnPaletteColor color(const QColor &color) const;
    QnPaletteColor color(const QString &group, int index) const;

private:
    void removeColors(const QString &group);
    void addColors(const QString &group, const QnColorList &colors);

private:
    QHash<QString, QnColorList> m_colorsByGroup;
    QHash<QColor, QnPaletteColor> m_colorByQColor;
};
