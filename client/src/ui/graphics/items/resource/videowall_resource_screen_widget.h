#ifndef VIDEOWALL_RESOURCE_SCREEN_WIDGET_H
#define VIDEOWALL_RESOURCE_SCREEN_WIDGET_H

#include <core/resource/resource_fwd.h>

#include <core/resource/videowall_pc_data.h>

#include "resource_widget.h"

class QnVideoWallItem;
class QnCameraThumbnailManager;

/** Class for displaying single screen of the videowall resource on the scene. */
class QnVideowallResourceScreenWidget : public QnResourceWidget
{
    Q_OBJECT
    typedef QnResourceWidget base_type;

public:
    QnVideowallResourceScreenWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    virtual ~QnVideowallResourceScreenWidget();

    /**
     * \returns                         Videowall associated with this widget.
     */
    const QnVideoWallResourcePtr &videowall() const;
protected:
    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;

private:
    void updateLayout();

private slots:
    void at_thumbnailReady(int resourceId, const QPixmap &thumbnail);

private:
    friend class LayoutOverlayWidget;

    QnVideoWallResourcePtr m_videowall;
    QList<QnVideoWallItem> m_items;
    QList<QnVideoWallPcData::PcScreen> m_screens;

    /** Geometry of the target screen set. */
    QRect m_desktopGeometry;

    QnCameraThumbnailManager *m_thumbnailManager;
    QHash<int, QPixmap> m_thumbs;
};

#endif // VIDEOWALL_RESOURCE_SCREEN_WIDGET_H
