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
    LayoutOverlayWidget(const QnLayoutResourcePtr &layout, QnVideowallResourceScreenWidget *parent, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags),
        m_widget(parent),
        m_layout(layout)
    {

    }

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        Q_UNUSED(widget)
        if (!m_layout) {
            painter->fillRect(option->rect, Qt::red);
        }
        else {
            painter->fillRect(option->rect, Qt::blue);

            QRectF bounding;
            foreach (const QnLayoutItemData &data, m_layout->getItems()) {
                QRectF itemRect = data.combinedGeometry;
                if (!itemRect.isValid())
                    continue;
                bounding = bounding.united(itemRect);
            }

            if (bounding.isNull())
                return;

            qreal x = option->rect.left();
            qreal y = option->rect.top();

            qreal xspace = m_layout->cellSpacing().width() * 0.5;
            qreal yspace = m_layout->cellSpacing().height() * 0.5;

            qreal xscale, yscale, xoffset, yoffset;
            qreal sourceAr = m_layout->cellAspectRatio() * bounding.width() / bounding.height();
            qreal targetAr = option->rect.width() / option->rect.height();
            if (sourceAr > targetAr) {
                xscale = option->rect.width() / bounding.width();
                yscale = xscale / m_layout->cellAspectRatio();
                xoffset = 0;

                qreal h = bounding.height() * yscale;
                yoffset = (option->rect.height() - h) * 0.5 + option->rect.top();
            } else {
                yscale = option->rect.height() / bounding.height();
                xscale = yscale * m_layout->cellAspectRatio();
                yoffset = 0;

                qreal w = bounding.width() * xscale;
                xoffset = (option->rect.width() - w) * 0.5 + option->rect.top();
            }

        #ifdef RECT_DEBUG
            QPen pen(Qt::red);
            pen.setWidth(15);
            painter->setPen(pen);
        #endif

            foreach (const QnLayoutItemData &data, m_layout->getItems()) {
                QRectF itemRect = data.combinedGeometry;
                if (!itemRect.isValid())
                    continue;

                // cell bounds
                qreal x1 = (itemRect.left() - bounding.left() + xspace) * xscale + xoffset;
                qreal y1 = (itemRect.top() - bounding.top() + yspace) * yscale + yoffset;
                qreal w1 = (itemRect.width() - xspace*2) * xscale;
                qreal h1 = (itemRect.height() - yspace*2) * yscale;

                paintItem(painter, QRectF(x + x1, y + y1, w1, h1), data);
        #ifdef RECT_DEBUG
                painter->drawRect(QRectF(x + x1, y + y1, w1, h1));
        #endif
            }
        }
    }

private:
    void paintItem(QPainter *painter, const QRectF &paintRect, const QnLayoutItemData &data) {
        QnResourcePtr resource = (data.resource.id.isValid())
                ? qnResPool->getResourceById(data.resource.id)
                : qnResPool->getResourceByUniqId(data.resource.path);

        bool isServer = resource && (resource->flags() & QnResource::server);

        if (isServer && !m_widget->m_thumbs.contains(resource->getId())) {
            m_widget->m_thumbs[resource->getId()] = qnSkin->pixmap("events/thumb_server.png");
        } //TODO: #GDM VW local files placeholder

        if (resource && m_widget->m_thumbs.contains(resource->getId())) {
            QPixmap pixmap = m_widget->m_thumbs[resource->getId()];


            qreal targetAr = paintRect.width() / paintRect.height();
            qreal sourceAr = isServer
                    ? targetAr
                    : (qreal)pixmap.width() / pixmap.height();

            qreal x, y, w, h;
            if (sourceAr > targetAr) {
                w = paintRect.width();
                x = paintRect.left();
                h = w / sourceAr;
                y = (paintRect.height() - h) * 0.5 + paintRect.top();
            } else {
                h = paintRect.height();
                y = paintRect.top();
                w = h * sourceAr;
                x = (paintRect.width() - w) * 0.5 + paintRect.left();
            }

            if (!qFuzzyIsNull(data.rotation)) {
                QnScopedPainterTransformRollback guard(painter); Q_UNUSED(guard);
                painter->translate(paintRect.center());
                painter->rotate(data.rotation);
                painter->translate(-paintRect.center());
                painter->drawPixmap(QRectF(x, y, w, h).toRect(), pixmap);
            } else {
                painter->drawPixmap(QRectF(x, y, w, h).toRect(), pixmap);
            }
            return;
        }

        if (resource && (resource->flags() & QnResource::live_cam) && resource.dynamicCast<QnNetworkResource>()) {
            m_widget->m_thumbnailManager->selectResource(resource);
        }

        {
//            QnScopedPainterPenRollback(painter, QPen(Qt::gray, 15));
//            painter->drawRect(paintRect);
        }
    }

private:
    QnVideowallResourceScreenWidget* m_widget;
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
