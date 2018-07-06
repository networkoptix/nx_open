#pragma once

#include <QtGui/QFont>
#include <QtGui/QPixmap>

namespace nx {
namespace client {
namespace desktop {

class HighlightedAreaTextPainter
{
public:
    HighlightedAreaTextPainter();

    QFont nameFont() const;
    void setNameFont(const QFont& font);

    QFont valueFont() const;
    void setValueFont(const QFont& font);

    QColor color() const;
    void setColor(const QColor& color);

    QPixmap paintText(const QString& text) const;

private:
    QFont m_nameFont;
    QFont m_valueFont;
    QColor m_color;
};

} // namespace desktop
} // namespace client
} // namespace nx
