#pragma once

#include <QtWidgets/QTabBar>

namespace style
{
    class Metrics
    {
    public:
        static const int kMenuItemHPadding;
        static const int kMenuItemVPadding;
        static const int kMenuItemTextLeftPadding;
        static const int kArrowSize;
        static const int kCheckIndicatorSize;
        static const int kExclusiveIndicatorSize;
        static const int kMenuCheckSize;
        static const int kMinimumButtonWidth;
        static const int kButtonHeight;
        static const int kHeaderSize;
        static const int kViewRowHeight;
        static const QSize kSwitchSize;
    };

    class Colors
    {
    public:
        enum Palette
        {
            kBase,
            kContrast,
            kBlue,
            kGreen,
            kBrang
        };

        static QString paletteName(Palette palette);
    };

    class Properties
    {
    public:
        static const char *kHoveredRowProperty;
    };

    qreal dpr(qreal value);
    int dp(qreal value);

    bool isDark(const QColor &color);
    bool isTabRounded(QTabBar::Shape shape);

    class RectCoordinates
    {
        QRectF rect;

    public:
        RectCoordinates(const QRectF &rect);
        qreal x(qreal x);
        qreal y(qreal y);
    };

    enum Direction
    {
        Up,
        Down,
        Left,
        Right
    };
}
