#ifndef VIDEOWALL_RESOURCE_SCREEN_WIDGET_H
#define VIDEOWALL_RESOURCE_SCREEN_WIDGET_H

#include <core/resource/resource_fwd.h>

#include "resource_widget.h"

/** Class for displaying single screen of the videowall resource on the scene. */
class QnVideowallResourceScreenWidget : public QnResourceWidget
{
    Q_OBJECT
    typedef QnResourceWidget base_type;

public:
    QnVideowallResourceScreenWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    virtual ~QnVideowallResourceScreenWidget();

protected:
    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;

};

#endif // VIDEOWALL_RESOURCE_SCREEN_WIDGET_H
