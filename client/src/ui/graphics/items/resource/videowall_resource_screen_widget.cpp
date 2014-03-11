#include "videowall_resource_screen_widget.h"

#include <QtWidgets/QGraphicsAnchorLayout>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/layout_item_data.h>
#include <core/resource_management/resource_pool.h>

#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/workbench/workbench_item.h>

#include <ui/style/skin.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>


// -------------------------------------------------------------------------- //
// StatisticsOverlayWidget
// -------------------------------------------------------------------------- //
class LayoutOverlayWidget: public GraphicsWidget {
    typedef GraphicsWidget base_type;
public:
    LayoutOverlayWidget(const QnLayoutResourcePtr &layout, QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags),
        m_layout(layout)
    {

    }

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        Q_UNUSED(widget)
        if (!m_layout)
            painter->fillRect(option->rect, Qt::red);
        else
            painter->fillRect(option->rect, Qt::blue);
    }

private:
    const QnLayoutResourcePtr m_layout;
};


// -------------------------------------------------------------------------- //
// QnVideowallResourceScreenWidget
// -------------------------------------------------------------------------- //

QnVideowallResourceScreenWidget::QnVideowallResourceScreenWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(context, item, parent)
{
    setOption(QnResourceWidget::WindowRotationForbidden, true);

    m_videowall = base_type::resource().dynamicCast<QnVideoWallResource>(); //TODO: #GDM VW check null in all usage places
    if(!m_videowall)
        qnCritical("QnVideowallResourceScreenWidget was created with a non-videowall resource.");

    QUuid pcUuid = item->data(Qn::VideoWallPcGuidRole).value<QUuid>();
    QnVideoWallPcData pc = m_videowall->getPc(pcUuid);  //TODO: #GDM VW check null in all usage places

    QList<int> screenIndices = item->data(Qn::VideoWallPcScreenIndicesRole).value<QList<int> >();
    qDebug() << screenIndices;

    //TODO: #GDM VW if pc list updated or a single pc changed, videowall review layout must be invalidated
//    connect(m_videowall, &QnVideoWallResource::pcChanged)

    foreach(const QnVideoWallPcData::PcScreen &screen, pc.screens) {
        if (!screenIndices.contains(screen.index))
            continue;
        m_screens << screen;
        m_desktopGeometry = m_desktopGeometry.united(screen.desktopGeometry);
    }

    if (m_desktopGeometry.isValid())    //TODO: #GDM VW and if not?
        setAspectRatio((qreal)m_desktopGeometry.width() / m_desktopGeometry.height());

    foreach (const QnVideoWallItem &item, m_videowall->getItems()) {
        if (item.pcUuid != pcUuid)
            continue;
        if (!m_desktopGeometry.contains(item.geometry))
            continue;
        m_items << item;
    }
    updateLayout();

    m_thumbnailManager = new QnCameraThumbnailManager(this);
    connect(m_thumbnailManager, &QnCameraThumbnailManager::thumbnailReady, this, &QnVideowallResourceScreenWidget::at_thumbnailReady);

    updateButtonsVisibility();
    updateTitleText();
    updateInfoText();
}

QnVideowallResourceScreenWidget::~QnVideowallResourceScreenWidget() {
}

const QnVideoWallResourcePtr &QnVideowallResourceScreenWidget::videowall() const {
    return m_videowall;
}

Qn::RenderStatus QnVideowallResourceScreenWidget::paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) {
    Q_UNUSED(channel)
    Q_UNUSED(channelRect)

    if (!paintRect.isValid())
        return Qn::NothingRendered;

    painter->fillRect(paintRect, Qt::black);
//    qreal offset = qMin(paintRect.width(), paintRect.height()) * 0.02;
//    QRectF contentsRect = paintRect.adjusted(offset, offset, -offset, -offset);
//    painter->fillRect(contentsRect, Qt::black);


    return Qn::NewFrameRendered;
}

void QnVideowallResourceScreenWidget::updateLayout() {

    QGraphicsAnchorLayout *layout = new QGraphicsAnchorLayout();
    layout->setContentsMargins(0.5, 0.5, 0.5, 0.5);
    layout->setSpacing(0.5);

    // can have several items on a single screen
    if (m_screens.size() == 1) {
        foreach (const QnVideoWallItem &item, m_items) {
            LayoutOverlayWidget *itemWidget = new LayoutOverlayWidget(qnResPool->getResourceById(item.layout).dynamicCast<QnLayoutResource>(), this);
            itemWidget->setAcceptedMouseButtons(Qt::NoButton);

            if (item.geometry.left() == m_desktopGeometry.left())
                layout->addAnchor(itemWidget, Qt::AnchorLeft, layout, Qt::AnchorLeft);
            else
                layout->addAnchor(itemWidget, Qt::AnchorLeft, layout, Qt::AnchorHorizontalCenter);

            if (item.geometry.top() == m_desktopGeometry.top())
                layout->addAnchor(itemWidget, Qt::AnchorTop, layout, Qt::AnchorTop);
            else
                layout->addAnchor(itemWidget, Qt::AnchorTop, layout, Qt::AnchorVerticalCenter);

            if (item.geometry.right() == m_desktopGeometry.right())
                layout->addAnchor(itemWidget, Qt::AnchorRight, layout, Qt::AnchorRight);
            else
                layout->addAnchor(itemWidget, Qt::AnchorRight, layout, Qt::AnchorHorizontalCenter);

            if (item.geometry.bottom() == m_desktopGeometry.bottom())
                layout->addAnchor(itemWidget, Qt::AnchorBottom, layout, Qt::AnchorBottom);
            else
                layout->addAnchor(itemWidget, Qt::AnchorBottom, layout, Qt::AnchorVerticalCenter);

        }
    } else if (m_items.size() == 1 ) {    // can have only on item on several screens
        LayoutOverlayWidget *itemWidget = new LayoutOverlayWidget(qnResPool->getResourceById(m_items.first().layout).dynamicCast<QnLayoutResource>(), this);
        itemWidget->setAcceptedMouseButtons(Qt::NoButton);

         //TODO: #GDM VW maybe just place this item as an overlay?
        layout->addAnchors(itemWidget, layout, Qt::Horizontal | Qt::Vertical);

    } else {
        qWarning() << "inconsistent videowall or no items on screens";
    }


    QnViewportBoundWidget *mainOverlayWidget = new QnViewportBoundWidget(this);
    mainOverlayWidget->setLayout(layout);
    mainOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);
    mainOverlayWidget->setOpacity(1.0);
    addOverlayWidget(mainOverlayWidget, UserVisible, true);
}

void QnVideowallResourceScreenWidget::at_thumbnailReady(int resourceId, const QPixmap &thumbnail) {
    m_thumbs[resourceId] = thumbnail;
    update();
}
