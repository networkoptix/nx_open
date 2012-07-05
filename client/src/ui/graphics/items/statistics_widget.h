#ifndef QN_STATISTICS_WIDGET_H
#define QN_STATISTICS_WIDGET_H

#include <QtCore/QWeakPointer>
#include <QtCore/QVector>
#include <QtCore/QMetaType>
#include <QtGui/QStaticText>
#include <QtGui/QGraphicsWidget>

#include <camera/render_status.h>
#include <core/resource/motion_window.h>
#include <core/resource/resource_consumer.h>
#include <core/datapacket/mediadatapacket.h> /* For QnMetaDataV1Ptr. */

#include <ui/common/constrained_resizable.h>
#include <ui/common/geometry.h>
#include <ui/common/frame_section_queryable.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/graphics/instruments/instrumented.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/statistics_widget_renderer.h>

#include "polygonal_shadow_item.h"

class QnStatisticsWidget: public QnResourceWidget {
public:
    QnStatisticsWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL):
      QnResourceWidget(context, item, parent){}
    virtual void initRenderer() override{
        m_renderer = new QnStatisticsWidgetRenderer(m_channelCount);
        connect(m_renderer, SIGNAL(sourceSizeChanged(const QSize &)), this, SLOT(at_sourceSizeChanged(const QSize &)));
        m_display->addRenderer(m_renderer);
	}
};

#endif // QN_STATISTICS_WIDGET_H
