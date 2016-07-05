#pragma once

#include <QtCore/QHash>
#include <QtGui/QColor>

typedef QList<QColor> QnColorList;

class QnGenericPalette;

class QnPaletteColor
{
public:
    static const int kMaxAlpha;

    explicit QnPaletteColor(QColor fallbackColor = QColor());

    QnPaletteColor(const QString &group,
                   const int index,
                   const QnColorList &palette,
                   const int alpha = kMaxAlpha);

    QString group() const;
    int index() const;
    QColor color() const;

    int alpha() const;
    void setAlpha(int alpha);
    void setAlphaF(qreal alpha);

    void setFallbackColor(QColor fallbackColor);

    bool isValid() const;

    QnPaletteColor darker(int shift) const;
    QnPaletteColor lighter(int shift) const;

    operator QColor() const;

    QnPaletteColor &operator =(const QnPaletteColor &other);

private:
    QString m_group;
    int m_index;
    QnColorList m_palette;
    int m_alpha;
    QColor m_fallbackColor;
};

class QnGenericPalette
{
public:
    QnGenericPalette();

    QnColorList colors(const QString &group) const;
    void setColors(const QString &group, const QnColorList &colors);

    QnPaletteColor color(const QColor &color) const;
    QnPaletteColor color(const QString &group, int index, int alpha = QnPaletteColor::kMaxAlpha) const;

private:
    void removeColors(const QString &group);
    void addColors(const QString &group, const QnColorList &colors);

private:
    QHash<QString, QnColorList> m_colorsByGroup;
    QHash<QColor, QnPaletteColor> m_colorByQColor;
};
