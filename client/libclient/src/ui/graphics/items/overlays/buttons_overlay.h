
#pragma once

#include <ui/graphics/items/generic/viewport_bound_widget.h>

class GraphicsLabel;
class QnImageButtonBar;

class QnButtonsOverlay : public QnViewportBoundWidget
{
public:
    QnButtonsOverlay(QGraphicsItem *parent = nullptr);

    virtual ~QnButtonsOverlay();

    void setSimpleMode(bool isSimpleMode);

    QnImageButtonBar *leftButtonsBar();

    QnImageButtonBar *rightButtonsBar();

    GraphicsLabel *titleLabel();

    GraphicsLabel *extraInfoLabel();

private:
    QnImageButtonBar * const m_leftButtonsPanel;
    QnImageButtonBar * const m_rightButtonsPanel;
    GraphicsLabel * const m_nameLabel;
    GraphicsLabel * const m_extraInfoLabel;
};