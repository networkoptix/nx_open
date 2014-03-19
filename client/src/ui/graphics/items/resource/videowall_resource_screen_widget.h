#ifndef VIDEOWALL_RESOURCE_SCREEN_WIDGET_H
#define VIDEOWALL_RESOURCE_SCREEN_WIDGET_H

#include <core/resource/resource_fwd.h>

#include <core/resource/videowall_pc_data.h>

#include "resource_widget.h"

class QnVideoWallItem;
class QnCameraThumbnailManager;
class QGraphicsAnchorLayout;

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

    void at_videoWall_itemAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);

private:
    friend class QnLayoutResourceOverlayWidget;

    QnVideoWallResourcePtr m_videowall;
    QUuid m_pcUuid;
    QList<QnVideoWallItem> m_items;
    QList<QnVideoWallPcData::PcScreen> m_screens;

    QGraphicsAnchorLayout *m_mainLayout;

    /** Geometry of the target screen set. */
    QRect m_desktopGeometry;

    bool m_layoutUpdateRequired;


    QnCameraThumbnailManager *m_thumbnailManager;
    QHash<int, QPixmap> m_thumbs;
};

#endif // VIDEOWALL_RESOURCE_SCREEN_WIDGET_H
