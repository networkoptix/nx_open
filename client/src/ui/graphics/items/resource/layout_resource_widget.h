#ifndef QN_LAYOUT_RESOURCE_WIDGET_H
#define QN_LAYOUT_RESOURCE_WIDGET_H

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_item_index.h>

#include "resource_widget.h"

class QnCameraThumbnailManager;

class QnLayoutResourceWidget: public QnResourceWidget {
    Q_OBJECT
    typedef QnResourceWidget base_type;

public:
    QnLayoutResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    virtual ~QnLayoutResourceWidget();

    /**
     * \returns                         Resource associated with this widget.
     */
    const QnLayoutResourcePtr &resource() const;

    QnVideoWallItemIndexList linkedVideoWalls() const;

protected:
    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;

    void paintItem(QPainter *painter, const QRectF &paintRect, const QnLayoutItemData &data);

    virtual QString calculateTitleText() const override;
    virtual QString calculateInfoText() const override;

    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const;
private slots:
    void at_thumbnailReady(int resourceId, const QPixmap &thumbnail);

private:
    /** Layout resource. */
    QnLayoutResourcePtr m_resource;

    QnCameraThumbnailManager *m_thumbnailManager;
    QHash<int, QPixmap> m_thumbs;
};

#endif // QN_LAYOUT_RESOURCE_WIDGET_H
