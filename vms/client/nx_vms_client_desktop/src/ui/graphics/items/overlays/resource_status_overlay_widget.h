// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/graphics/items/generic/viewport_bound_widget.h>

class QLabel;
class QPushButton;

namespace nx::vms::client::desktop { class BusyIndicatorGraphicsWidget; }

class QnStatusOverlayWidget: public GraphicsWidget
{
    Q_OBJECT
    typedef GraphicsWidget base_type;

public:
    QnStatusOverlayWidget(QGraphicsWidget *parent = nullptr);

    enum class Control
    {
        kNoControl      = 0x00,
        kPreloader      = 0x01,
        kImageOverlay   = 0x02,
        kIcon           = 0x04,
        kCaption        = 0x08,
        kButton         = 0x10,
        kCustomButton   = 0x20,
    };
    Q_DECLARE_FLAGS(Controls, Control);

    enum class ErrorStyle
    {
        red,
        white,
    };

    virtual void paint(QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    void setVisibleControls(Controls controls);

    void setIconOverlayPixmap(const QPixmap& pixmap);

    void setIcon(const QPixmap& pixmap);
    void setUseErrorStyle(bool useErrorStyle);
    void setErrorStyle(ErrorStyle errorStyle);
    void setCaption(const QString& caption);
    void setButtonText(const QString& text);
    void setCustomButtonText(const QString& text);
    void setTooltip(const QString& tooltip) {} //< TODO: zkarakas
    void setShowGlow(bool showGlow);

signals:
    void actionButtonClicked();
    void customButtonClicked();
    void closeButtonClicked();

private:
    void setupPreloader();
    void setupCentralControls();
    void setupExtrasControls();
    void initializeHandlers();
    void updateAreaSizes();

private:
    Controls m_visibleControls;
    int m_visibleExtrasControlsCount = 0;
    bool m_initialized;
    bool m_useErrorStyle;
    ErrorStyle m_errorStyle;
    bool m_showGlow = false;

    QnViewportBoundWidget* const m_preloaderHolder;
    QnViewportBoundWidget* const m_centralHolder;
    QnViewportBoundWidget* const m_extrasHolder;

    QWidget* const m_centralContainer;
    QWidget* const m_extrasContainer;

    // Preloader
    nx::vms::client::desktop::BusyIndicatorGraphicsWidget* m_preloader;

    // Icon overlay
    QGraphicsPixmapItem m_imageItem;

    // Central area
    QLabel* const m_centralAreaImage;
    QLabel* const m_caption;

    // Extras
    QPushButton* const m_button;
    QPushButton* const m_customButton;

    QPixmap m_whiteGlowHorizontal;
    QPixmap m_whiteGlowVertical;
    QPixmap m_redGlowHorizontal;
    QPixmap m_redGlowVertical;
};
