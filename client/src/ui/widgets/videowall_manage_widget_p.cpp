#include "videowall_manage_widget_p.h"

#include <QtGui/QIcon>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>

#include <utils/common/string.h>
#include <utils/common/scoped_painter_rollback.h>

#include <utils/math/color_transformations.h>


namespace {
    const qreal frameMargin = 0.04;
    const qreal innerMargin = frameMargin * 2;

    const qreal minOpacity = 0.5;

    const int fillColorAlpha = 180;
    const int roundness = 25;
    const int borderWidth = 25;


    const QColor desktopColor(130, 130, 130);
    const QColor freeSpaceColor(64, 130, 180);
    const QColor existingItemColor(100, 100, 100);
    const QColor addedItemColor(64, 180, 130);
}

/************************************************************************/
/* BaseModelItem                                                        */
/************************************************************************/

QnVideowallManageWidgetPrivate::BaseModelItem::BaseModelItem(const QRect &rect, ItemType itemType, const QUuid &id):
    id(id),
    itemType(itemType),
    geometry(rect),
    flags(StateFlag::Default),
    opacity(minOpacity)
{}

void QnVideowallManageWidgetPrivate::BaseModelItem::paint(QPainter* painter) const {
    qreal margin = innerMargin * qMin(geometry.width(), geometry.height());

    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRoundRect(targetRect, roundness);

    QPen borderPen(baseColor);
    borderPen.setWidth(borderWidth);

    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, withAlpha(baseColor, fillColorAlpha));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawPath(path);
}

/************************************************************************/
/* FreeSpaceItem                                                        */
/************************************************************************/

QnVideowallManageWidgetPrivate::FreeSpaceItem::FreeSpaceItem(const QRect &rect, ItemType itemType):
    base_type(rect, itemType, QUuid::createUuid())
{
    baseColor = freeSpaceColor;
}

void QnVideowallManageWidgetPrivate::FreeSpaceItem::paint(QPainter* painter) const {
    if (!free())
        return;

    base_type::paint(painter);

    // for now allow adding of only one item to simplify the logic
    //if (!m_added.isEmpty())
    //    return;

    QRect iconRect(0, 0, 200, 200);
    iconRect.moveCenter(geometry.center());
    painter->drawPixmap(iconRect, qnSkin->pixmap("item/ptz_zoom_in.png"));
}

/************************************************************************/
/* ModelScreenPart                                                      */
/************************************************************************/

QnVideowallManageWidgetPrivate::ModelScreenPart::ModelScreenPart(int screenIdx, int xIndex, int yIndex, const QRect &rect) :
    base_type(rect, ItemType::ScreenPart),
    m_free(true)
{
    for (auto snap = snaps.values.begin(); snap != snaps.values.end(); ++snap) {
        snap->screenIndex = screenIdx;
    }
    snaps.left().snapIndex = xIndex;
    snaps.top().snapIndex = yIndex;
    snaps.right().snapIndex = QnScreenSnap::snapsPerScreen() - xIndex - 1;
    snaps.bottom().snapIndex = QnScreenSnap::snapsPerScreen() - yIndex - 1;
}

bool QnVideowallManageWidgetPrivate::ModelScreenPart::free() const {
    return m_free;
}

void QnVideowallManageWidgetPrivate::ModelScreenPart::setFree(bool value) {
    m_free = value;
}

/************************************************************************/
/* ModelScreen                                                          */
/************************************************************************/

QnVideowallManageWidgetPrivate::ModelScreen::ModelScreen(int idx, const QRect &rect) :
    base_type(rect, ItemType::Screen)
{
    for (auto snap = snaps.values.begin(); snap != snaps.values.end(); ++snap) {
        snap->screenIndex = idx;
        snap->snapIndex = 0;
    }

    int width = rect.width() / QnScreenSnap::snapsPerScreen();
    int height = rect.height() / QnScreenSnap::snapsPerScreen();

    for (int j = 0; j < QnScreenSnap::snapsPerScreen(); ++j) {
        for (int k = 0; k < QnScreenSnap::snapsPerScreen(); ++k) {
            QRect partRect(rect.x() + width * j, rect.y() + height * k, width, height);
            ModelScreenPart part(idx, j, k, partRect);
            parts << part;
        } 
    }
}

bool QnVideowallManageWidgetPrivate::ModelScreen::free() const {
    return std::all_of(parts.cbegin(), parts.cend(), [](const ModelScreenPart &part) {return part.free();});
}

void QnVideowallManageWidgetPrivate::ModelScreen::setFree(bool value) {
    for (auto part = parts.begin(); part != parts.end(); ++part)
        part->setFree(value);
}

void QnVideowallManageWidgetPrivate::ModelScreen::paint(QPainter* painter) const {
    {
        qreal margin = frameMargin * qMin(geometry.width(), geometry.height());
        QRectF targetRect = eroded(geometry, margin);

        QPainterPath path;
        path.addRect(targetRect);

        QPen borderPen(desktopColor);
        borderPen.setWidth(10);

        QnScopedPainterPenRollback penRollback(painter, borderPen);
        QnScopedPainterBrushRollback brushRollback(painter, withAlpha(desktopColor, 127));

        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);

        painter->drawPath(path);
    }
    base_type::paint(painter);   
}

/************************************************************************/
/* ModelItem                                                            */
/************************************************************************/

QnVideowallManageWidgetPrivate::ModelItem::ModelItem(ItemType itemType, const QUuid &id):
    BaseModelItem(QRect(), itemType, id)
{}

bool QnVideowallManageWidgetPrivate::ModelItem::free() const {
    return false;
}

void QnVideowallManageWidgetPrivate::ModelItem::setFree(bool value) {
    Q_UNUSED(value);
    assert("Should never get here");
}

/************************************************************************/
/* ExistingItem                                                         */
/************************************************************************/

QnVideowallManageWidgetPrivate::ExistingItem::ExistingItem(const QUuid &id) :
    ModelItem(ItemType::Existing, id)
{
    baseColor = existingItemColor;
}

/************************************************************************/
/* AddedItem                                                            */
/************************************************************************/

QnVideowallManageWidgetPrivate::AddedItem::AddedItem() :
    ModelItem(ItemType::Added, QUuid::createUuid())
{
    baseColor = addedItemColor;
}

/************************************************************************/
/* QnVideowallManageWidgetPrivate                                       */
/************************************************************************/

QnVideowallManageWidgetPrivate::QnVideowallManageWidgetPrivate() {
    QDesktopWidget* desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); ++i) {
        ModelScreen screen(i, desktop->screenGeometry(i));
        m_screens << screen;

        m_unitedGeometry = m_unitedGeometry.united(screen.geometry);
    }
}

//TODO: #GDM #VW create iterators
void QnVideowallManageWidgetPrivate::foreachItem(std::function<void(BaseModelItem& /*item*/, bool& /*abort*/)> handler) {
    bool abort = false;
    for (auto screen = m_screens.begin(); screen != m_screens.end(); ++screen) {
        handler(*screen, abort);
        if (abort)
            return;

        // do not iterate over an empty screen parts
        if (screen->free())
            continue;

        for (auto part = screen->parts.begin(); part != screen->parts.end(); ++part) {
            handler(*part, abort);
            if (abort)
                return;
        }
    }

    for (auto item = m_items.begin(); item != m_items.end(); ++item) {
        handler(*item, abort);
        if (abort)
            return;
    }

    for (auto added = m_added.begin(); added != m_added.end(); ++added) {
        handler(*added, abort);
        if (abort)
            return;
    }

}

void QnVideowallManageWidgetPrivate::foreachItemConst(std::function<void(const BaseModelItem& /*item*/, bool& /*abort*/)> handler) {
    bool abort = false;
    for (auto screen = m_screens.cbegin(); screen != m_screens.cend(); ++screen) {
        handler(*screen, abort);
        if (abort)
            return;

        // do not iterate over an empty screen parts
        if (screen->free())
            continue;

        for (auto part = screen->parts.cbegin(); part != screen->parts.cend(); ++part) {
            handler(*part, abort);
            if (abort)
                return;
        }
    }

    for (auto item = m_items.cbegin(); item != m_items.cend(); ++item) {
        handler(*item, abort);
        if (abort)
            return;
    }

    for (auto added = m_added.cbegin(); added != m_added.cend(); ++added) {
        handler(*added, abort);
        if (abort)
            return;
    }
}

void QnVideowallManageWidgetPrivate::loadFromResource(const QnVideoWallResourcePtr &videowall) {

    QList<QRect> screens;
    QDesktopWidget* desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); ++i)
        screens << desktop->screenGeometry(i);

    foreach (const QnVideoWallItem &item, videowall->items()->getItems()) {
        ExistingItem modelItem(item.uuid);
        modelItem.snaps = item.screenSnaps;
        modelItem.geometry = item.screenSnaps.geometry(screens);
        m_items << modelItem;
        use(modelItem.snaps);
    }
}

void QnVideowallManageWidgetPrivate::submitToResource(const QnVideoWallResourcePtr &videowall) {

    QUuid pcUuid = qnSettings->pcUuid();

    QList<QnVideoWallPcData::PcScreen> localScreens;
    for (int i = 0; i < m_screens.size(); i++) {
        QnVideoWallPcData::PcScreen screen;
        screen.index = i;
        screen.desktopGeometry = m_screens[i].geometry;
        localScreens << screen;
    }

    QnVideoWallPcData pcData;
    pcData.uuid = pcUuid;
    pcData.screens = localScreens;
    if (!videowall->pcs()->hasItem(pcUuid))
        videowall->pcs()->addItem(pcData);
    else
        videowall->pcs()->updateItem(pcUuid, pcData);

    foreach (const ModelItem &addedItem, m_added) {
        QnVideoWallItem item;
        item.name = generateUniqueString([&videowall] () {
            QStringList used;
            foreach (const QnVideoWallItem &item, videowall->items()->getItems())
                used << item.name;
            return used;
        }(), tr("Screen"), tr("Screen %1") );
        item.pcUuid = pcUuid;
        item.uuid = addedItem.id;
        item.screenSnaps = addedItem.snaps;
        videowall->items()->addItem(item);
    }
}

QTransform QnVideowallManageWidgetPrivate::getTransform(const QRect &rect)
{
    if (m_widgetRect == rect)
        return m_transform;

    m_transform.reset();
    m_transform.translate(rect.left(), rect.top());
    m_transform.scale(
        static_cast<qreal> (rect.width()) /  m_unitedGeometry.width(),
        static_cast<qreal> (rect.height()) /  m_unitedGeometry.height());
    m_transform.translate(-m_unitedGeometry.left(), -m_unitedGeometry.top());
    m_invertedTransform = m_transform.inverted();

    m_widgetRect = rect;
    return m_transform;
}

QTransform QnVideowallManageWidgetPrivate::getInvertedTransform(const QRect &rect)
{
    if (m_widgetRect != rect)
        getTransform(rect);
    return m_invertedTransform;
}

QRect QnVideowallManageWidgetPrivate::targetRect(const QRect &rect) const
{
    if (m_unitedGeometry.isNull())    //TODO: #GDM #VW replace by model.Valid
        return QRect();

    return expanded(aspectRatio(m_unitedGeometry), rect, Qt::KeepAspectRatio).toRect();
}

void QnVideowallManageWidgetPrivate::use(const QnScreenSnaps &snaps)
{
    QSet<int> screenIdxs = snaps.screens();
    // if the item takes some screens, it should take them fully
    if (screenIdxs.size() > 1) {
        for (int idx: screenIdxs) {
            if (idx >= m_screens.size())
                continue;   // we can lose one screen after snaps have been set
            assert(m_screens[idx].free());
            m_screens[idx].setFree(false);
        }
    } else if (!screenIdxs.isEmpty()) { //otherwise looking at the screen parts
        int screenIdx = screenIdxs.toList().first();
        if (screenIdx >= m_screens.size())
            return;
        ModelScreen &screen = m_screens[screenIdx];

        for (int i = snaps.left().snapIndex; i < QnScreenSnap::snapsPerScreen() - snaps.right().snapIndex; ++i) {
            for (int j = snaps.top().snapIndex; j < QnScreenSnap::snapsPerScreen() - snaps.bottom().snapIndex; ++j) {
                int idx = i* QnScreenSnap::snapsPerScreen() + j;
                assert(idx < screen.parts.size());
                assert(screen.parts[idx].free());
                screen.parts[idx].setFree(false);
            }
        }
    }
}

void QnVideowallManageWidgetPrivate::paint(QPainter* painter, const QRect &rect) {
    QRect targetRect = this->targetRect(rect);
    if (targetRect.isEmpty())
        return;

    // transform to paint in the desktop coordinate system
    QTransform transform(getTransform(targetRect));

    QnScopedPainterTransformRollback transformRollback(painter, transform);

    foreachItemConst([this, painter](const BaseModelItem &item, bool &) {
        QnScopedPainterOpacityRollback opacityRollback(painter, item.opacity);
        item.paint(painter);
    });
}

void QnVideowallManageWidgetPrivate::mouseMoveAt(const QPoint &pos, Qt::MouseButtons buttons) {
    foreachItem([pos](BaseModelItem &item, bool &) {
        if (item.geometry.contains(pos))
            item.flags |= StateFlag::Hovered;
        else
            item.flags &= ~StateFlags(StateFlag::Hovered);
    });
}

void QnVideowallManageWidgetPrivate::mouseClickAt(const QPoint &pos, Qt::MouseButtons buttons) {
    if ((buttons & Qt::LeftButton) != Qt::LeftButton)
        return;

    foreachItem([this, pos](BaseModelItem &item, bool &abort) {
        if (!item.free())
            return;

        AddedItem added;
        added.geometry = item.geometry;
        added.snaps = item.snaps;
        added.flags |= StateFlag::Hovered;   //mouse cursor is over it
        m_added << added;
        item.setFree(false);
        abort = true;
    });
}

void QnVideowallManageWidgetPrivate::dragStartAt(const QPoint &pos) {

}

void QnVideowallManageWidgetPrivate::dragMoveAt(const QPoint &pos) {

}

void QnVideowallManageWidgetPrivate::dragEndAt(const QPoint &pos) {

}

void QnVideowallManageWidgetPrivate::tick(int deltaMSecs) {
    qreal opacityDelta = 0.001 * deltaMSecs;

    foreachItem([this, opacityDelta](BaseModelItem &item, bool &) {
        if (item.flags & StateFlag::Hovered)
            item.opacity = qMin(item.opacity + opacityDelta, 1.0);
        else
            item.opacity = qMax(item.opacity - opacityDelta, minOpacity);
    });
}
