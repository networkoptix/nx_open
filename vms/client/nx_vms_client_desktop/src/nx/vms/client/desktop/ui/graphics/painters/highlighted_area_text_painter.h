// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QFont>
#include <QtGui/QPixmap>

namespace nx::vms::client::desktop {

class HighlightedAreaTextPainter
{
public:
    HighlightedAreaTextPainter();

    struct Fonts
    {
        QFont title;
        QFont name;
        QFont value;

        bool operator==(const Fonts& other) const
        {
            return title == other.title
                && name == other.name
                && value == other.value;
        }
    };
    Fonts fonts() const;
    void setFonts(const Fonts& fonts);

    QColor color() const;
    void setColor(const QColor& color);

    QPixmap paintText(const QString& text, qreal devicePixelRatio) const;

private:
    Fonts m_fonts;
    QColor m_color;
};

} // namespace nx::vms::client::desktop
