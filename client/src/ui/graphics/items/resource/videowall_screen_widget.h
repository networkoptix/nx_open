#ifndef VIDEOWALL_RESOURCE_SCREEN_WIDGET_H
#define VIDEOWALL_RESOURCE_SCREEN_WIDGET_H

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_pc_data.h>
#include <utils/common/id.h>

#include <ui/graphics/items/resource/resource_widget.h>

class QnVideoWallItem;
class QnCameraThumbnailManager;
class QGraphicsAnchorLayout;
class QGraphicsWidget;

/** Class for displaying single screen of the videowall resource on the scene. */
class QnVideowallScreenWidget : public QnResourceWidget
{
    Q_OBJECT
    typedef QnResourceWidget base_type;

public:
    QnVideowallScreenWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    virtual ~QnVideowallScreenWidget();

    /**
     * \returns                         Videowall associated with this widget.
     */
    const QnVideoWallResourcePtr &videowall() const;

    typedef QHash<QUuid, quint64> ReviewButtons;
protected:
    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;

    virtual Buttons calculateButtonsVisibility() const override;
    virtual QString calculateTitleText() const override;
private:
    void updateLayout();
private slots:
    void at_thumbnailReady(const QnId &resourceId, const QPixmap &thumbnail);

    void at_videoWall_itemAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);

private:
    friend class QnVideowallItemWidget;

    QnVideoWallResourcePtr m_videowall;
    QUuid m_pcUuid;
    QList<QnVideoWallItem> m_items;
    QList<QnVideoWallPcData::PcScreen> m_screens;

    QGraphicsWidget* m_mainOverlayWidget;
    QGraphicsAnchorLayout *m_mainLayout;

    /** Geometry of the target screen set. */
    QRect m_desktopGeometry;

    bool m_layoutUpdateRequired;


    QnCameraThumbnailManager *m_thumbnailManager;
    QHash<QnId, QPixmap> m_thumbs;
};

#endif // VIDEOWALL_RESOURCE_SCREEN_WIDGET_H
