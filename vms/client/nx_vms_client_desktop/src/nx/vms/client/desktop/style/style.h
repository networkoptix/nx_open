// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCommonStyle>

class QGroupBox;

namespace nx::vms::client::desktop {

class StylePrivate;

class Style: public QCommonStyle
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(Style)
    using base_type = QCommonStyle;

public:
    Style();
    virtual ~Style() override;

    void drawSwitch(
        QPainter* painter, const QStyleOption* option, const QWidget* widget = nullptr) const;

    virtual void drawPrimitive(PrimitiveElement element,
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget = 0) const override;
    virtual void drawComplexControl(ComplexControl control,
        const QStyleOptionComplex* option,
        QPainter* painter,
        const QWidget* widget) const override;
    virtual void drawControl(ControlElement element,
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget) const override;
    virtual QRect subControlRect(ComplexControl control,
        const QStyleOptionComplex* option,
        SubControl subControl,
        const QWidget* widget) const override;
    virtual QRect subElementRect(
        SubElement subElement, const QStyleOption* option, const QWidget* widget) const override;
    virtual QSize sizeFromContents(ContentsType type,
        const QStyleOption* option,
        const QSize& size,
        const QWidget* widget) const override;
    virtual int pixelMetric(
        PixelMetric metric, const QStyleOption* option, const QWidget* widget) const override;
    virtual int styleHint(StyleHint sh,
        const QStyleOption* option = nullptr,
        const QWidget* widget = nullptr,
        QStyleHintReturn* shret = nullptr) const override;
    virtual QIcon standardIcon(
        StandardPixmap iconId, const QStyleOption* option, const QWidget* widget) const override;
    virtual void polish(QWidget* widget) override;
    virtual void unpolish(QWidget* widget) override;

    /**
     * Paints cosmetic rectangular frame.
     * @painter a painter to paint with
     * @rect a rectangle to paint frame around
     * @color frame color
     * @frameWidth frame width, will not be transformed by painter transformation
     *     positive width produces inner frame, negative - outer frame
     * @frameShift frame shift from rect, will not be transformed by painter transformation
     *     positive value produces inward shift, negative - outward shift
     */
    static void paintCosmeticFrame(
        QPainter* painter,
        const QRectF& rect,
        const QColor& color,
        int frameWidth,
        int frameShift);

    static void setGroupBoxContentTopMargin(QGroupBox* box, int margin);

    static int sliderPositionFromValue(
        int min, int max, int logicalValue, int span, bool upsideDown = false);

    static Style* instance();

protected:
    virtual bool eventFilter(QObject* object, QEvent* event) override;
};

} // namespace nx::vms::client::desktop

#define qnStyle nx::vms::client::desktop::NxStyle::instance()
