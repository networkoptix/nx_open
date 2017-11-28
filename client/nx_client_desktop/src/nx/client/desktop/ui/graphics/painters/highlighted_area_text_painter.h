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

    QFont font() const;
    void setFont(const QFont& font);

    QColor color() const;
    void setColor(const QColor& color);

    QPixmap paintText(const QString& text) const;

private:
    QFont m_font;
    QColor m_color;
};

} // namespace desktop
} // namespace client
} // namespace nx
