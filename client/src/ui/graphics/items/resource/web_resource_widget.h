#pragma once

#include <ui/graphics/items/resource/resource_widget.h>

class QnWebResourceWidget : public QnResourceWidget {
    Q_OBJECT

    typedef QnResourceWidget base_type;
public:
    QnWebResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    virtual ~QnWebResourceWidget();

protected:
    virtual QString calculateTitleText() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;

    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;
};
