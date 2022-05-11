// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "ptz_instrument_p.h"

class PtzOverlayWidget: public GraphicsWidget
{
    Q_OBJECT
    typedef GraphicsWidget base_type;

public:
    PtzOverlayWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = {});

    void forceUpdateLayout();

    void hideCursor();

    void showCursor();

    PtzManipulatorWidget* manipulatorWidget() const;

    PtzImageButtonWidget* zoomInButton() const;

    PtzImageButtonWidget* zoomOutButton() const;

    PtzImageButtonWidget* focusInButton() const;

    PtzImageButtonWidget* focusOutButton() const;

    PtzImageButtonWidget* focusAutoButton() const;

    PtzImageButtonWidget* modeButton() const;

    Qt::Orientations markersMode() const;

    void setMarkersMode(Qt::Orientations mode);

    virtual void setGeometry(const QRectF& rect) override;

    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    qreal unit() const;

    void updateLayout();

private:
    Qt::Orientations m_markersMode;
    PtzManipulatorWidget* m_manipulatorWidget = nullptr;
    PtzImageButtonWidget* m_zoomInButton = nullptr;
    PtzImageButtonWidget* m_zoomOutButton = nullptr;
    PtzImageButtonWidget* m_focusInButton = nullptr;
    PtzImageButtonWidget* m_focusOutButton = nullptr;
    PtzImageButtonWidget* m_focusAutoButton = nullptr;
    PtzImageButtonWidget* m_modeButton = nullptr;
    const QPen m_pen;
};
