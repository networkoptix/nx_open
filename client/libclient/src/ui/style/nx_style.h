#pragma once

#include <QCommonStyle>

#include <ui/style/generic_palette.h>
#include <ui/common/geometry.h>

class QnNxStylePrivate;
class QnNxStyle : public QCommonStyle, public QnGeometry
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QnNxStyle)
    typedef QCommonStyle base_type;

public:
    class Colors
    {
    public:
        enum Palette
        {
            kBase,
            kContrast,
            kBlue,
            kGreen,
            kBrand,
            kRed,
            kYellow
        };

        static QString paletteName(Palette palette);
    };

public:
    QnNxStyle();

    void setGenericPalette(const QnGenericPalette &palette);
    const QnGenericPalette &genericPalette() const;


    QnPaletteColor findColor(const QColor &color) const;
    QnPaletteColor mainColor(Colors::Palette palette) const;

    void drawSwitch(
            QPainter *painter,
            const QStyleOption *option,
            const QWidget *widget = nullptr) const;

    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const override;
    virtual void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override;
    virtual QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const override;
    virtual QRect subElementRect(SubElement subElement, const QStyleOption *option, const QWidget *widget) const override;
    virtual QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const override;
    virtual int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override;
    virtual int styleHint(StyleHint sh, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *shret = nullptr) const override;
    virtual QPixmap standardPixmap(StandardPixmap iconId, const QStyleOption* option, const QWidget* widget) const override;
    virtual void polish(QWidget *widget) override;
    virtual void unpolish(QWidget *widget) override;

    static QnNxStyle *instance();

protected:
    virtual bool eventFilter(QObject* object, QEvent* event) override;

protected:
    QnNxStyle(QnNxStylePrivate &dd);
};
