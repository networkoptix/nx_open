#include "videowall_resource_screen_widget.h"

#include <camera/camera_thumbnail_manager.h>

#include <core/resource/resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/layout_item_data.h>
#include <core/resource_management/resource_pool.h>

#include <ui/workbench/workbench_item.h>

#include <ui/style/skin.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>


QnVideowallResourceScreenWidget::QnVideowallResourceScreenWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(context, item, parent)
{
    setOption(QnResourceWidget::WindowRotationForbidden, true);


}

QnVideowallResourceScreenWidget::~QnVideowallResourceScreenWidget() {
}

Qn::RenderStatus QnVideowallResourceScreenWidget::paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) {
    painter->fillRect(paintRect, Qt::red);
}
