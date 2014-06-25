#include "videowall_manage_widget_p.h"

#include <QtGui/QIcon>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>

#include <utils/common/string.h>
#include <utils/common/scoped_painter_rollback.h>


namespace {
    const qreal frameMargin = 0.04;
    const qreal innerMargin = frameMargin * 2;
}


QnVideowallManageWidgetPrivate::QnVideowallManageWidgetPrivate() {
    QDesktopWidget* desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); ++i) {
        ModelScreen screen(i, desktop->screenGeometry(i));
        screens << screen;

        unitedGeometry = unitedGeometry.united(screen.geometry);
    }
}

void QnVideowallManageWidgetPrivate::loadFromResource(const QnVideoWallResourcePtr &videowall) {

    QList<QRect> screens;
    QDesktopWidget* desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); ++i)
        screens << desktop->screenGeometry(i);

    foreach (const QnVideoWallItem &item, videowall->items()->getItems()) {
        ModelItem modelItem;
        modelItem.id = item.uuid;
        modelItem.itemType = ItemType::Existing;
        modelItem.snaps = item.screenSnaps;
        modelItem.geometry = item.screenSnaps.geometry(screens);
        items << modelItem;
        use(modelItem.snaps);
    }
}

void QnVideowallManageWidgetPrivate::submitToResource(const QnVideoWallResourcePtr &videowall) {

    QUuid pcUuid = qnSettings->pcUuid();

    QList<QnVideoWallPcData::PcScreen> localScreens;
    for (int i = 0; i < screens.size(); i++) {
        QnVideoWallPcData::PcScreen screen;
        screen.index = i;
        screen.desktopGeometry = screens[i].geometry;
        localScreens << screen;
    }

    QnVideoWallPcData pcData;
    pcData.uuid = pcUuid;
    pcData.screens = localScreens;
    if (!videowall->pcs()->hasItem(pcUuid))
        videowall->pcs()->addItem(pcData);
    else
        videowall->pcs()->updateItem(pcUuid, pcData);

    foreach (const ModelItem &addedItem, added) {
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


void QnVideowallManageWidgetPrivate::paintScreenFrame(QPainter *painter, const BaseModelItem &item)
{
    QRect geometry = item.geometry;

    qreal margin = frameMargin * qMin(geometry.width(), geometry.height());
    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRect(targetRect);

    QPen borderPen(QColor(130, 130, 130, 200));
    borderPen.setWidth(10);

    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, QColor(130, 130, 130, 128));

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);

    painter->drawPath(path);
}


void QnVideowallManageWidgetPrivate::paintPlaceholder(QPainter* painter, const BaseModelItem &item) {
    QRect geometry = item.geometry;

    qreal margin = innerMargin * qMin(geometry.width(), geometry.height());

    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRoundRect(targetRect, 25);

    QPen borderPen(QColor(64, 130, 180, 200));
    borderPen.setWidth(25);
    
    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, QColor(64, 130, 180, 128));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawPath(path);

    // for now allow adding of only one item to simplify the logic
    if (!added.isEmpty())
        return;

    QRect iconRect(0, 0, 200, 200);
    iconRect.moveCenter(targetRect.center().toPoint());
    painter->drawPixmap(iconRect, qnSkin->pixmap("item/ptz_zoom_in.png"));
}


void QnVideowallManageWidgetPrivate::paintExistingItem(QPainter* painter, const BaseModelItem &item) {
    QRect geometry = item.geometry;

    qreal margin = innerMargin * qMin(geometry.width(), geometry.height());

    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRoundRect(targetRect, 25);

    QPen borderPen(QColor(64, 64, 64, 200));
    borderPen.setWidth(25);

    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, QColor(64, 64, 64, 128));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawPath(path);

}

void QnVideowallManageWidgetPrivate::paintAddedItem(QPainter* painter, const BaseModelItem &item) {
    QRect geometry = item.geometry;

    qreal margin = innerMargin * qMin(geometry.width(), geometry.height());

    QRectF targetRect = eroded(geometry, margin);

    QPainterPath path;
    path.addRoundRect(targetRect, 25);

    QPen borderPen(QColor(64, 180, 130, 200));
    borderPen.setWidth(25);

    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, QColor(64, 180, 130, 128));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawPath(path);
}

QTransform QnVideowallManageWidgetPrivate::getTransform(const QRect &rect)
{
    if (m_widgetRect == rect)
        return m_transform;

    m_transform.reset();
    m_transform.translate(rect.left(), rect.top());
    m_transform.scale(
        static_cast<qreal> (rect.width()) /  unitedGeometry.width(),
        static_cast<qreal> (rect.height()) /  unitedGeometry.height());
    m_transform.translate(-unitedGeometry.left(), -unitedGeometry.top());
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
    if (unitedGeometry.isNull())    //TODO: #GDM #VW replace by model.Valid
        return QRect();

    return expanded(aspectRatio(unitedGeometry), rect, Qt::KeepAspectRatio).toRect();
}

QUuid QnVideowallManageWidgetPrivate::itemIdByPos(const QPoint &pos)
{
    foreach (const ModelItem &item, items) {
        if (!item.geometry.contains(pos))
            continue;
        return item.id;
    }

    foreach (const ModelScreen &screen, screens) {
        if (!screen.geometry.contains(pos))
            continue;
        if (screen.free())
            return screen.id;
        foreach (const ModelScreenPart &part, screen.parts) {
            if (!part.geometry.contains(pos))
                continue;
            return part.id;
        }
    }

    foreach (const ModelItem &item, added) {
        if (!item.geometry.contains(pos))
            continue;
        return item.id;
    }

    return QUuid();
}

void QnVideowallManageWidgetPrivate::addItemTo(const QUuid &id) {
    if (!added.isEmpty())
        return; //allow to add only one item for now

    for (auto screen = screens.begin(); screen != screens.end(); ++screen) {
        if (screen->id == id) {
            ModelItem item;
            item.id = QUuid::createUuid();
            item.itemType = ItemType::Added;
            item.geometry = screen->geometry;
            item.snaps = screen->snaps;
            added << item;
            screen->setFree(false);
            return;
        }

        if (screen->free())
            continue;

        for (auto part = screen->parts.begin(); part != screen->parts.end(); ++part) {
            if (part->id != id)
                continue;

            ModelItem item;
            item.id = QUuid::createUuid();
            item.itemType = ItemType::Added;
            item.geometry = part->geometry;
            item.snaps = part->snaps;
            added << item;
            part->free = false;
            return;
        }
    }
}

void QnVideowallManageWidgetPrivate::use(const QnScreenSnaps &snaps)
{
    QSet<int> screenIdxs = snaps.screens();
    // if the item takes some screens, it should take them fully
    if (screenIdxs.size() > 1) {
        for (int idx: screenIdxs) {
            if (idx >= screens.size())
                continue;   // we can lose one screen after snaps have been set
            assert(screens[idx].free());
            screens[idx].setFree(false);
        }
    } else if (!screenIdxs.isEmpty()) { //otherwise looking at the screen parts
        int screenIdx = screenIdxs.toList().first();
        if (screenIdx >= screens.size())
            return;
        ModelScreen &screen = screens[screenIdx];

        for (int i = snaps.left().snapIndex; i < QnScreenSnap::snapsPerScreen() - snaps.right().snapIndex; ++i) {
            for (int j = snaps.top().snapIndex; j < QnScreenSnap::snapsPerScreen() - snaps.bottom().snapIndex; ++j) {
                int idx = i* QnScreenSnap::snapsPerScreen() + j;
                assert(idx < screen.parts.size());
                assert(screen.parts[idx].free);
                screen.parts[idx].free = false;
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

    QList<BaseModelItem> smallItems, bigItems;
    foreach (const BaseModelItem &item, items) {
        if (item.snaps.screens().size() > 1)
            bigItems << item;
        else
            smallItems << item;
    }

    foreach (const BaseModelItem &item, bigItems) {
        paintExistingItem(painter, item);
    }

    foreach (const BaseModelItem &item, smallItems) {
        paintExistingItem(painter, item);
    }

    foreach (const ModelScreen &screen, screens) {
        paintScreenFrame(painter, screen);
        if (screen.free()) {
            paintPlaceholder(painter, screen);
        } else {
            foreach (const ModelScreenPart &part, screen.parts) {
                if (part.free)
                    paintPlaceholder(painter, part);
            }
        }
    }

    foreach (const ModelItem &item, added) {
        paintAddedItem(painter, item);
    }
 }

BaseModelItem::BaseModelItem(const QRect &rect):
    geometry(rect)
{}


ModelItem::ModelItem():
    BaseModelItem(QRect())
{}


ModelScreenPart::ModelScreenPart(int screenIdx, int xIndex, int yIndex, const QRect &rect) :
    BaseModelItem(rect),
    free(true)
{
    id = QUuid::createUuid();
    itemType = ItemType::ScreenPart;
    for (auto snap = snaps.values.begin(); snap != snaps.values.end(); ++snap) {
        snap->screenIndex = screenIdx;
    }
    snaps.left().snapIndex = xIndex;
    snaps.top().snapIndex = yIndex;
    snaps.right().snapIndex = QnScreenSnap::snapsPerScreen() - xIndex - 1;
    snaps.bottom().snapIndex = QnScreenSnap::snapsPerScreen() - yIndex - 1;
}

ModelScreen::ModelScreen(int idx, const QRect &rect) :
    BaseModelItem(rect)
{
    id = QUuid::createUuid();
    itemType = ItemType::Screen;
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

bool ModelScreen::free() const {
    return std::all_of(parts.cbegin(), parts.cend(), [](const ModelScreenPart &part) {return part.free;});
}

void ModelScreen::setFree(bool value) {
    std::transform(parts.begin(), parts.end(), parts.begin(), [value](ModelScreenPart part) {
        part.free = value;
        return part;
    });
}
