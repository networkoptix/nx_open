// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QFont>
#include <QtWidgets/QGraphicsItem>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/types.h>
#include <nx/vms/client/desktop/ui/graphics/painters/highlighted_area_text_painter.h>

namespace nx::vms::client::desktop {

class AreaTooltipItem: public QGraphicsItem
{
    using base_type = QGraphicsItem;

public:
    AreaTooltipItem(QGraphicsItem* parent = nullptr);
    virtual ~AreaTooltipItem() override;

    QString text() const;
    void setText(const QString& text);

    using Fonts = HighlightedAreaTextPainter::Fonts;
    Fonts fonts() const;
    void setFonts(const Fonts& fonts);

    QColor textColor() const;
    void setTextColor(const QColor& color);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor& color);

    virtual QRectF boundingRect() const override;
    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget = nullptr) override;

    QMarginsF textMargins() const;

    void setFigure(const figure::FigurePtr& figure);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
