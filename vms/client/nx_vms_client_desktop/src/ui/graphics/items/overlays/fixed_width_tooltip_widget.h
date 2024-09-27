// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QTextDocument>

#include <ui/graphics/items/generic/tool_tip_widget.h>

class QnFixedWidthTooltipWidget: public QnToolTipWidget
{
    Q_OBJECT
    typedef QnToolTipWidget base_type;

public:
    QnFixedWidthTooltipWidget(QGraphicsItem* parent = nullptr);
    virtual ~QnFixedWidthTooltipWidget() = default;

    QString text() const;

    void setText(const QString& text, qreal lineHeight);
    void setTextWidth(qreal width);
    void setTextColor(const QColor& color);

protected:
    virtual void paint(
        QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    void updateHeight();
    void generateTextPixmap();

    QTextDocument m_textDocument;

    QString m_text;
    qreal m_textWidth = 1;
    qreal m_lineHeight = -1;
    qreal m_oldHeight = -1;
    QColor m_textColor = Qt::black;
    QPixmap m_textPixmap;
};
