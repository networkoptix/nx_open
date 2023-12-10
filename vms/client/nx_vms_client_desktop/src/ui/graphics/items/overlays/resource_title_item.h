// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsWidget>

class GraphicsLabel;
class QnImageButtonBar;

class QnResourceTitleItem: public QGraphicsWidget
{
    Q_OBJECT
    using base_type = QGraphicsWidget;

public:
    QnResourceTitleItem(QGraphicsItem* parent = nullptr);
    virtual ~QnResourceTitleItem();

    virtual void paint(QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    void setSimpleMode(bool isSimpleMode, bool animate);

    QnImageButtonBar* leftButtonsBar();
    QnImageButtonBar* rightButtonsBar();
    GraphicsLabel* titleLabel();
    GraphicsLabel* extraInfoLabel();

protected:
    virtual void resizeEvent(QGraphicsSceneResizeEvent* event) override;

private:
    QnImageButtonBar* const m_leftButtonsPanel;
    QnImageButtonBar* const m_rightButtonsPanel;
    GraphicsLabel* const m_nameLabel;
    GraphicsLabel* const m_extraInfoLabel;
    bool m_isSimpleMode = false;

    QPixmap m_backgroundCache;

    void updateNameLabelElideConstraint();
};
