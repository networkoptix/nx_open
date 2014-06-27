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

    const qreal opacityChangeSpeed = 0.002;
    const qreal draggedOpacity = 0.2;
    const qreal minOpacity = 0.5;

    const int fillColorAlpha = 180;
    const int roundness = 25;
    const int borderWidth = 25;
    const int transformationOffset = 50;

    const QColor desktopColor(Qt::darkGray);
    const QColor freeSpaceColor(64, 130, 180);
    const QColor existingItemColor(Qt::lightGray);
    const QColor addedItemColor(64, 180, 130);
    const QColor textColor(Qt::white);
}

/************************************************************************/
/* BaseModelItem                                                        */
/************************************************************************/

QnVideowallManageWidgetPrivate::BaseModelItem::BaseModelItem(const QRect &rect, ItemType itemType, const QUuid &id):
    id(id),
    itemType(itemType),
    geometry(rect),
    flags(StateFlag::Default),
    opacity(minOpacity),
    editable(false)
{}

void QnVideowallManageWidgetPrivate::BaseModelItem::paint(QPainter* painter) const {
    qreal margin = innerMargin * qMin(geometry.width(), geometry.height());

    QRectF targetRect = eroded(QRectF(geometry), margin);

    QPainterPath path;
    path.addRoundRect(targetRect, roundness);

    QPen borderPen(baseColor);
    borderPen.setWidth(borderWidth);

    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, withAlpha(baseColor, fillColorAlpha));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawPath(path);

    if (!name.isEmpty()) {
        QPen textPen(textColor);
        QnScopedPainterPenRollback penRollback(painter, textPen);

        QFont font(painter->font());
        font.setPixelSize(120);
        QnScopedPainterFontRollback fontRollback(painter, font);

        QRect textRect(0, 0,
            painter->fontMetrics().width(name),
            painter->fontMetrics().height());
        textRect.moveCenter(targetRect.center().toPoint());
        textRect.moveTop(targetRect.top() + targetRect.height() * 0.7);

        painter->drawText(textRect, name);
    }

    if (editable && hasFlag(StateFlag::Hovered)) {
        QRect iconRect(0, 0, 200, 200);
        iconRect.moveCenter(geometry.center());
        painter->drawPixmap(iconRect, qnSkin->pixmap("item/move.png"));

        {
            QPen editBorderPen(textColor);
            editBorderPen.setWidth(borderWidth);

            QVector<qreal> dashes;
            dashes << 2 << 3;
            editBorderPen.setDashPattern(dashes);

            QnScopedPainterPenRollback penRollback(painter, editBorderPen);
            QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
            painter->drawRect(targetRect);
        }

        /*  // Debug painting of the transformation anchors
        {
            QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
            QnScopedPainterBrushRollback brushRollback(painter, Qt::red);
            {
                QRect innerRect(eroded(geometry, transformationOffset * 2));
                painter->drawRect(dilated(QRect(innerRect.left(), innerRect.top(), 0, innerRect.height()), transformationOffset));
                painter->drawRect(dilated(QRect(innerRect.center().x(), innerRect.top(), 0, innerRect.height()), transformationOffset));
                painter->drawRect(dilated(QRect(innerRect.right(), innerRect.top(), 0, innerRect.height()), transformationOffset));

                painter->drawRect(dilated(QRect(innerRect.left(), innerRect.top(), innerRect.width(), 0), transformationOffset));
                painter->drawRect(dilated(QRect(innerRect.left(), innerRect.center().y(), innerRect.width(), 0), transformationOffset));
                painter->drawRect(dilated(QRect(innerRect.left(), innerRect.bottom(), innerRect.width(), 0), transformationOffset));
            }
        }
        */

        {
            QPen resizeBorderPen(textColor);
            resizeBorderPen.setWidth(borderWidth / 2);

            QnScopedPainterPenRollback penRollback(painter, resizeBorderPen);
            QnScopedPainterBrushRollback brushRollback(painter, baseColor);

            QRect resizeRect(0, 0, borderWidth*3, borderWidth*3);

            resizeRect.moveCenter(QPoint(targetRect.left(), targetRect.top()));
            painter->drawRect(resizeRect);

            resizeRect.moveCenter(QPoint(targetRect.right(), targetRect.top()));
            painter->drawRect(resizeRect);

            resizeRect.moveCenter(QPoint(targetRect.left(), targetRect.bottom()));
            painter->drawRect(resizeRect);

            resizeRect.moveCenter(QPoint(targetRect.right(), targetRect.bottom()));
            painter->drawRect(resizeRect);

            resizeRect.moveCenter(QPoint(targetRect.left(), targetRect.center().y()));
            painter->drawRect(resizeRect);

            resizeRect.moveCenter(QPoint(targetRect.right(), targetRect.center().y()));
            painter->drawRect(resizeRect);

            resizeRect.moveCenter(QPoint(targetRect.center().x(), targetRect.top()));
            painter->drawRect(resizeRect);

            resizeRect.moveCenter(QPoint(targetRect.center().x(), targetRect.bottom()));
            painter->drawRect(resizeRect);
        }

 
    }
}

bool QnVideowallManageWidgetPrivate::BaseModelItem::hasFlag(StateFlags flag) const {
    return (flags & flag) == flag;
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
    name = tr("Desktop %1").arg(idx + 1);

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
        QRectF targetRect = eroded(QRectF(geometry), margin);

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
{
}

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
    editable = true;
    baseColor = addedItemColor;
}

/************************************************************************/
/* QnVideowallManageWidgetPrivate                                       */
/************************************************************************/

QnVideowallManageWidgetPrivate::QnVideowallManageWidgetPrivate(QWidget *q):
    q_ptr(q),
    m_process(ItemTransformation::None)
{
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
        modelItem.name = item.name;
        modelItem.snaps = item.screenSnaps;
        modelItem.geometry = item.screenSnaps.geometry(screens);
        m_items << modelItem;
        setFree(modelItem.snaps, false);
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

void QnVideowallManageWidgetPrivate::setFree(const QnScreenSnaps &snaps, bool value)
{
    QSet<int> screenIdxs = snaps.screens();
    // if the item takes some screens, it should take them fully
    if (screenIdxs.size() > 1) {
        for (int idx: screenIdxs) {
            if (idx >= m_screens.size())
                continue;   // we can lose one screen after snaps have been set
            assert(m_screens[idx].free() != value);
            m_screens[idx].setFree(value);
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
                assert(screen.parts[idx].free() != value);
                screen.parts[idx].setFree(value);
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

void QnVideowallManageWidgetPrivate::mouseMoveAt(const QPoint &pos) {
    if (m_process != ItemTransformation::None)
        return;

    ItemTransformations proposed = ItemTransformation::None;
    foreachItem([this, pos, &proposed](BaseModelItem &item, bool &) {
        if (item.geometry.contains(pos)) {
            item.flags |= StateFlag::Hovered;
            if (item.editable && m_process == ItemTransformation::None) {
                proposed = transformationsAnchor(item.geometry, pos);
            }
        }
        else
            item.flags &= ~StateFlags(StateFlag::Hovered);
    });

    QCursor cursor(transformationsCursor(proposed));

    Q_Q(QWidget);
    q->setCursor(cursor);
}

void QnVideowallManageWidgetPrivate::mouseClickAt(const QPoint &pos, Qt::MouseButtons buttons) {
    if ((buttons & Qt::LeftButton) != Qt::LeftButton)
        return;
    if (m_process != ItemTransformation::None)
        return;

    foreachItem([this, pos](BaseModelItem &item, bool &abort) {
        if (!item.free() || !item.geometry.contains(pos))
            return;

        AddedItem added;
        added.name = tr("New Item");
        added.geometry = item.geometry;
        added.snaps = item.snaps;
        added.flags |= StateFlag::Hovered;   //mouse cursor is over it
        added.opacity = 1.0;
        m_added << added;
        item.setFree(false);
        abort = true;
    });
}

void QnVideowallManageWidgetPrivate::dragStartAt(const QPoint &pos, const QPoint &oldPos) {
    assert(m_process == ItemTransformation::None);
    foreachItem([this, pos](BaseModelItem &item, bool &abort) {
        if (!item.editable || !item.geometry.contains(pos))
            return;
        abort = true;   //no more than one such item must exist

        m_process = transformationsAnchor(item.geometry, pos);
        if (m_process == ItemTransformation::None) 
            return;

        if (m_process == ItemTransformation::Move) 
            moveItemStart(item);
        else
            resizeItemStart(item);       
    });
}

void QnVideowallManageWidgetPrivate::dragMoveAt(const QPoint &pos, const QPoint &oldPos) {
    if (m_process == ItemTransformation::None)
        return;

    foreachItem([this, &pos, &oldPos](BaseModelItem &item, bool &abort) {
        if (!item.hasFlag(StateFlag::Pressed))
            return;
        QPoint offset = pos - oldPos;
        if (m_process == ItemTransformation::Move)
            moveItemMove(item, offset);
        else
            resizeItemMove(item, offset);
        abort = true;
    });
}

void QnVideowallManageWidgetPrivate::dragEndAt(const QPoint &pos, const QPoint &oldPos) {
    if (m_process == ItemTransformation::None)
        return;

    foreachItem([this, pos](BaseModelItem &item, bool &abort) {
        item.flags &= ~StateFlags(StateFlag::Hovered);
        if (!item.hasFlag(StateFlag::Pressed))
            return;
        if (m_process == ItemTransformation::Move)
            moveItemEnd(item);
        else
            resizeItemEnd(item);
    });
    m_process = ItemTransformation::None;
}

void QnVideowallManageWidgetPrivate::tick(int deltaMSecs) {
    qreal opacityDelta = opacityChangeSpeed * deltaMSecs;

    foreachItem([this, opacityDelta](BaseModelItem &item, bool &) {
        if (item.hasFlag(StateFlag::Pressed))
            item.opacity = qMax(item.opacity - opacityDelta, draggedOpacity);
        else
        if (item.hasFlag(StateFlag::Hovered) && (m_process == ItemTransformation::None))
            item.opacity = qMin(item.opacity + opacityDelta, 1.0);
        else
            item.opacity = qMax(item.opacity - opacityDelta, minOpacity);
    });
}

void QnVideowallManageWidgetPrivate::moveItemStart(BaseModelItem &item) {
    item.flags |= StateFlag::Pressed;
    setFree(item.snaps, true);

}

void QnVideowallManageWidgetPrivate::moveItemMove(BaseModelItem &item, const QPoint &offset) {
    item.geometry.translate(offset);
}

void QnVideowallManageWidgetPrivate::moveItemEnd(BaseModelItem &item) {
    item.flags &= ~StateFlags(StateFlag::Pressed);

    if (item.snaps.screens().size() > 1) {
        assert("Not implemented");
    } else {
        int bestIndex = -1;
        qint64 bestCoeff = 0;
        for (int i = 0; i < m_screens.size(); ++i) {
            ModelScreen screen = m_screens[i]; 
            if (!screen.free())
                continue;
            QSize intersectionSize = m_screens[i].geometry.intersected(item.geometry).size();
            qint64 coeff = intersectionSize.width() * intersectionSize.height();
            if (bestIndex < 0 || coeff > bestCoeff) {
                bestIndex = i;
                bestCoeff = coeff;
            }
        }
        assert(bestIndex >= 0);

        ModelScreen screen = m_screens[bestIndex]; 
        item.geometry = screen.geometry;
        item.snaps = screen.snaps;
    }

    //for bes user experience do not hide controls until user moves a mouse
    item.flags |= StateFlags(StateFlag::Hovered);
    setFree(item.snaps, false);
}

void QnVideowallManageWidgetPrivate::resizeItemStart(BaseModelItem &item) {
    setFree(item.snaps, true);
    item.flags |= StateFlag::Pressed;
}

void QnVideowallManageWidgetPrivate::resizeItemMove(BaseModelItem &item, const QPoint &offset) {
    if (m_process & ItemTransformation::ResizeLeft)
        item.geometry.adjust(offset.x(), 0, 0, 0);
    else if (m_process & ItemTransformation::ResizeRight)
        item.geometry.adjust(0, 0, offset.x(), 0);

    if (m_process & ItemTransformation::ResizeTop)
        item.geometry.adjust(0, offset.y(), 0, 0);
    else if (m_process & ItemTransformation::ResizeBottom)
        item.geometry.adjust(0, 0, 0, offset.y());
}

void QnVideowallManageWidgetPrivate::resizeItemEnd(BaseModelItem &item) {
    item.flags &= ~StateFlags(StateFlag::Pressed);
    setFree(item.snaps, false);
}

QnVideowallManageWidgetPrivate::ItemTransformations QnVideowallManageWidgetPrivate::transformationsAnchor(const QRect &geometry, const QPoint &pos) const {
    if (!geometry.contains(pos))
        return ItemTransformation::None;

    QRect innerRect(eroded(geometry, transformationOffset * 2));

    QRect leftAnchor(dilated(QRect(innerRect.left(), innerRect.top(), 0, innerRect.height()), transformationOffset));
    QRect centerXAnchor(dilated(QRect(innerRect.center().x(), innerRect.top(), 0, innerRect.height()), transformationOffset));
    QRect rightAnchor(dilated(QRect(innerRect.left() + innerRect.width(), innerRect.top(), 0, innerRect.height()), transformationOffset));

    QRect topAnchor(dilated(QRect(innerRect.left(), innerRect.top(), innerRect.width(), 0), transformationOffset));
    QRect centerYAnchor(dilated(QRect(innerRect.left(), innerRect.center().y(), innerRect.width(), 0), transformationOffset));
    QRect bottomAnchor(dilated(QRect(innerRect.left(), innerRect.top() + innerRect.height(), innerRect.width(), 0), transformationOffset));

    auto in = [pos](const QRect &first, const QRect &second) {
        return first.contains(pos) && second.contains(pos);
    };

    if (in(centerXAnchor, centerYAnchor))
        return ItemTransformation::Move;
    if (in(leftAnchor, centerYAnchor))
        return ItemTransformation::ResizeLeft;
    if (in(rightAnchor, centerYAnchor))
        return ItemTransformation::ResizeRight;
    if (in(topAnchor, centerXAnchor))
        return ItemTransformation::ResizeTop;
    if (in(bottomAnchor, centerXAnchor))
        return ItemTransformation::ResizeBottom;
    if (in(leftAnchor, topAnchor))
        return ItemTransformation::ResizeLeft | ItemTransformation::ResizeTop;
    if (in(leftAnchor, bottomAnchor))
        return ItemTransformation::ResizeLeft | ItemTransformation::ResizeBottom;
    if (in(rightAnchor, topAnchor))
        return ItemTransformation::ResizeRight | ItemTransformation::ResizeTop;
    if (in(rightAnchor, bottomAnchor))
        return ItemTransformation::ResizeRight | ItemTransformation::ResizeBottom;

    return ItemTransformation::None;
}

Qt::CursorShape QnVideowallManageWidgetPrivate::transformationsCursor(ItemTransformations value) const {
    switch (value) {
    case ItemTransformation::ResizeLeft | ItemTransformation::ResizeTop:
    case ItemTransformation::ResizeRight | ItemTransformation::ResizeBottom:
        return Qt::SizeFDiagCursor;
    case ItemTransformation::ResizeRight | ItemTransformation::ResizeTop:
    case ItemTransformation::ResizeLeft | ItemTransformation::ResizeBottom:
        return Qt::SizeBDiagCursor;
    case ItemTransformation::ResizeLeft:
    case ItemTransformation::ResizeRight:
        return Qt::SizeHorCursor;
    case ItemTransformation::ResizeTop:
    case ItemTransformation::ResizeBottom:
        return Qt::SizeVerCursor;
    case ItemTransformation::Move:
        return Qt::SizeAllCursor;
    case ItemTransformation::None:
        return Qt::ArrowCursor;
    default:
        qWarning() << "Invalid transformation" << value;
        return Qt::ArrowCursor;
    }
}
