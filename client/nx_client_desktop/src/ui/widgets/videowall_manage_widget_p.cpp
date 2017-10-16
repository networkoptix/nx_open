#include "videowall_manage_widget_p.h"

#include <QtGui/QIcon>
#include <QtGui/QScreen>

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>
#include <ui/widgets/videowall_manage_widget.h>
#include <ui/dialogs/common/message_box.h>

#include <nx/utils/string.h>
#include <utils/common/scoped_painter_rollback.h>

#include <utils/math/color_transformations.h>
#include <utils/math/linear_combination.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/utils/app_info.h>

using nx::client::core::Geometry;

namespace {

const int frameMargin = 40;
const int bodyMargin = 80;

const qreal opacityChangeSpeed = 0.002;
const qreal draggedOpacity = 0.2;
const qreal minOpacity = 0.5;

const qreal minAreaOverlapping = 0.25;
const qreal textOffset = 0.7;
const qreal partScreenCoeff = 0.65;
const qreal deleteButtonOffset = 0.16;

const int fillColorAlpha = 180;
const int screenBackgroundAlpha = 127;
const int roundness = 25;
const int borderWidth = 25;
const int resizeBorderWidth = 12;
const int resizeAnchorSize = 75;
const int screenBorderWidth = 10;
const int transformationOffset = 50;
const int baseIconSize = 200;
const int baseFontSize = 120;

QVector<qreal> initDashPattern()
{
    return QVector<qreal>() << 2 << 3;
}

const QVector<qreal> dashPattern(initDashPattern());

int pixelRatio()
{
    return nx::utils::AppInfo::isMacOsX()
        ? 1 //< MacOs automatically manages dpi.
        : qApp->devicePixelRatio();
}

QList<QRect> screenGeometries()
{
    QList<QRect> result;

    if (nx::utils::AppInfo::isMacOsX())
    {
        for (const auto screen: QGuiApplication::screens())
            result.append(screen->geometry());
    }
    else
    {
        const auto desktop = qApp->desktop();
        for (int screenNumber = 0; screenNumber < desktop->screenCount(); ++screenNumber)
        {
            if (const auto screen = desktop->screen(screenNumber))
            {
                const auto rect = screen->geometry();
                auto pixelRatio = screen->devicePixelRatio();
                result.append(QRect(rect.topLeft(), rect.size() * pixelRatio));
            }
            else
            {
                NX_ASSERT(false, "Invalid screen.");
            }
        }
    }
    return result;
}

} // namespace

/************************************************************************/
/* BaseModelItem                                                        */
/************************************************************************/

QnVideowallManageWidgetPrivate::BaseModelItem::BaseModelItem(const QRect &geometry, ItemType itemType, const QnUuid &id, QnVideowallManageWidget* q):
    id(id),
    itemType(itemType),
    geometry(geometry),
    flags(StateFlag::Default),
    opacity(minOpacity),
    deleteButtonOpacity(0.0),
    editable(false),
    q_ptr(q)
{}

QnVideowallManageWidgetPrivate::BaseModelItem::~BaseModelItem() {}


bool QnVideowallManageWidgetPrivate::BaseModelItem::hasFlag(StateFlags flag) const {
    return (flags & flag) == flag;
}

QRect QnVideowallManageWidgetPrivate::BaseModelItem::bodyRect() const {
    QRect body = Geometry::eroded(geometry, bodyMargin);
    if (!isPartOfScreen())
        return body;
    int offset = bodyMargin * partScreenCoeff;

    if (snaps.left().snapIndex == 0) {
        if (snaps.right().snapIndex != 0) {
            body.translate(offset, 0);
        }
        else {
            body.setLeft(body.left() + offset);
            body.setRight(body.right() - offset);
        }
    }
    else if (snaps.right().snapIndex == 0)
        body.translate(-offset, 0);

    if (snaps.top().snapIndex == 0) {
        if (snaps.bottom().snapIndex != 0) {
            body.translate(0, offset);
        }
        else {
            body.setTop(body.top() + offset);
            body.setBottom(body.bottom() - offset);
        }
    }
    else if (snaps.bottom().snapIndex == 0)
        body.translate(0, -offset);

    return body;
}

QRect QnVideowallManageWidgetPrivate::BaseModelItem::deleteButtonRect() const {
    QRect iconRect(0, 0, iconSize()*0.7, iconSize()*0.7);

    QPoint p = bodyRect().topRight();
    p.setX(p.x() - transformationOffset* partScreenCoeff);
    p.setY(p.y() + transformationOffset* partScreenCoeff);

    iconRect.moveTopRight(p);

    return iconRect;
}

QPainterPath QnVideowallManageWidgetPrivate::BaseModelItem::bodyPath() const {
    QPainterPath path;
    path.addRoundRect(bodyRect(), roundness);
    return path;
}

int QnVideowallManageWidgetPrivate::BaseModelItem::fontSize() const
{
    return isPartOfScreen()
        ? baseFontSize * partScreenCoeff * pixelRatio()
        : baseFontSize * pixelRatio();
}

int QnVideowallManageWidgetPrivate::BaseModelItem::iconSize() const
{
    return isPartOfScreen()
        ? baseIconSize * partScreenCoeff * pixelRatio()
        : baseIconSize * pixelRatio();
}

void QnVideowallManageWidgetPrivate::BaseModelItem::paint(QPainter* painter, const TransformationProcess &process) const {
    QRect body = bodyRect();

    QPen borderPen(baseColor());
    borderPen.setWidth(borderWidth);

    QnScopedPainterPenRollback penRollback(painter, borderPen);
    QnScopedPainterBrushRollback brushRollback(painter, withAlpha(baseColor(), fillColorAlpha));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    painter->drawPath(bodyPath());

    if (!name.isEmpty()) {
        Q_Q(const QnVideowallManageWidget);
        QPen textPen(q->colors().text);
        QnScopedPainterPenRollback penRollback(painter, textPen);

        QFont font(painter->font());
        font.setPixelSize(fontSize());
        QnScopedPainterFontRollback fontRollback(painter, font);

        const auto metrics = QFontMetrics(font);
        auto textRect = metrics.boundingRect(name);
        textRect.moveCenter(body.center());
        textRect.moveTop(body.top() + body.height() * textOffset);

        painter->drawText(textRect, Qt::TextDontClip, name);
    }

    if (editable && hasFlag(StateFlag::Hovered)) {
#ifdef DEBUG_RESIZE_ANCHORS
        {
            QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
            QnScopedPainterBrushRollback brushRollback(painter, Qt::red);

            QRect innerRect(bodyRect());

            painter->drawRect(dilated(QRect(innerRect.left(), innerRect.top(), 0, innerRect.height()), transformationOffset));
            painter->drawRect(dilated(QRect(innerRect.center().x(), innerRect.top(), 0, innerRect.height()), transformationOffset));
            painter->drawRect(dilated(QRect(innerRect.left() + innerRect.width(), innerRect.top(), 0, innerRect.height()), transformationOffset));

            painter->drawRect(dilated(QRect(innerRect.left(), innerRect.top(), innerRect.width(), 0), transformationOffset));
            painter->drawRect(dilated(QRect(innerRect.left(), innerRect.center().y(), innerRect.width(), 0), transformationOffset));
            painter->drawRect(dilated(QRect(innerRect.left(), innerRect.top() + innerRect.height(), innerRect.width(), 0), transformationOffset));
        }
#endif
        paintPixmap(painter, body, qnSkin->pixmap("videowall_settings/move.png"));
        paintDeleteButton(painter);

        QPainterPath anchorPath;
        anchorPath.addRect(body);
        paintDashBorder(painter, anchorPath);
        paintResizeAnchors(painter, body);
    }
    else if (process.isRunning() && geometry.intersects(process.geometry) && !hasFlag(StateFlag::Pressed))
        paintProposed(painter, process.geometry);
}

void QnVideowallManageWidgetPrivate::BaseModelItem::paintDashBorder(QPainter *painter, const QPainterPath &path) const {
    Q_Q(const QnVideowallManageWidget);
    QPen pen(q->colors().text);
    pen.setWidth(borderWidth);
    pen.setDashPattern(dashPattern);

    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
    painter->drawPath(path);
}


void QnVideowallManageWidgetPrivate::BaseModelItem::paintPixmap(QPainter *painter, const QRect &rect, const QPixmap &pixmap) const {
    QRect iconRect(0, 0, iconSize(), iconSize());
    iconRect.moveCenter(rect.center());
    painter->drawPixmap(iconRect, pixmap);
}

void QnVideowallManageWidgetPrivate::BaseModelItem::paintDeleteButton(QPainter *painter) const {
    Q_Q(const QnVideowallManageWidget);
    QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);

    QColor color = linearCombine(deleteButtonOpacity, q->colors().error, 1.0 - deleteButtonOpacity, q->colors().text);
    QnScopedPainterBrushRollback brushRollback(painter, color);

    QRect rect = deleteButtonRect();
    int offset = rect.width() * deleteButtonOffset;

    {
        QPainterPath path;
        path.moveTo(rect.width(), offset);
        path.lineTo(rect.width() - offset, 0);
        path.lineTo(0, rect.height() - offset);
        path.lineTo(offset, rect.height());
        path.closeSubpath();
        path.translate(rect.topLeft());
        painter->drawPath(path);
    }
    // setFillMode does not work o_O, using minor hack --gdm
    {
        QPainterPath path;
        path.moveTo(0, offset);
        path.lineTo(offset, 0);
        path.lineTo(rect.width(), rect.height() - offset);
        path.lineTo(rect.width() - offset, rect.height());
        path.closeSubpath();
        path.translate(rect.topLeft());
        painter->drawPath(path);
    }

}

void QnVideowallManageWidgetPrivate::BaseModelItem::paintResizeAnchors(QPainter *painter, const QRect &rect) const {
    Q_Q(const QnVideowallManageWidget);
    QPen resizeBorderPen(q->colors().text);
    resizeBorderPen.setWidth(resizeBorderWidth);

    QnScopedPainterPenRollback penRollback(painter, resizeBorderPen);
    QnScopedPainterBrushRollback brushRollback(painter, baseColor());

    QRect resizeRect(0, 0, resizeAnchorSize, resizeAnchorSize);

    resizeRect.moveCenter(QPoint(rect.left(), rect.top()));
    painter->drawRect(resizeRect);

    resizeRect.moveCenter(QPoint(rect.right(), rect.top()));
    painter->drawRect(resizeRect);

    resizeRect.moveCenter(QPoint(rect.left(), rect.bottom()));
    painter->drawRect(resizeRect);

    resizeRect.moveCenter(QPoint(rect.right(), rect.bottom()));
    painter->drawRect(resizeRect);

    resizeRect.moveCenter(QPoint(rect.left(), rect.center().y()));
    painter->drawRect(resizeRect);

    resizeRect.moveCenter(QPoint(rect.right(), rect.center().y()));
    painter->drawRect(resizeRect);

    resizeRect.moveCenter(QPoint(rect.center().x(), rect.top()));
    painter->drawRect(resizeRect);

    resizeRect.moveCenter(QPoint(rect.center().x(), rect.bottom()));
    painter->drawRect(resizeRect);
}

bool QnVideowallManageWidgetPrivate::BaseModelItem::isPartOfScreen() const {
    return std::any_of(snaps.values.cbegin(), snaps.values.cend(), [](const QnScreenSnap &snap) {return snap.snapIndex > 0;});
}

void QnVideowallManageWidgetPrivate::BaseModelItem::paintProposed(QPainter* painter, const QRect &proposedGeometry) const {
    Q_UNUSED(proposedGeometry)
    paintDashBorder(painter, bodyPath());
}

/************************************************************************/
/* FreeSpaceItem                                                        */
/************************************************************************/

QnVideowallManageWidgetPrivate::FreeSpaceItem::FreeSpaceItem(const QRect &rect, ItemType itemType, QnVideowallManageWidget* q):
    base_type(rect, itemType, QnUuid::createUuid(), q)
{}

void QnVideowallManageWidgetPrivate::FreeSpaceItem::paint(QPainter* painter, const TransformationProcess &process) const {
    if (!free())
        return;
    base_type::paint(painter, process);
    if (!process.isRunning())
        paintPixmap(painter, bodyRect(), qnSkin->pixmap("buttons/plus.png"));
}

QColor QnVideowallManageWidgetPrivate::FreeSpaceItem::baseColor() const {
    Q_Q(const QnVideowallManageWidget);
    return q->colors().freeSpace;
}

/************************************************************************/
/* ModelScreenPart                                                      */
/************************************************************************/

QnVideowallManageWidgetPrivate::ModelScreenPart::ModelScreenPart(int screenIdx, int xIndex, int yIndex, const QRect &rect, QnVideowallManageWidget* q) :
    base_type(rect, ItemType::ScreenPart, q),
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

QnVideowallManageWidgetPrivate::ModelScreen::ModelScreen(int idx, const QRect &rect, QnVideowallManageWidget* q) :
    base_type(rect, ItemType::Screen, q)
{
    name = tr("Display %1").arg(idx + 1);

    for (auto snap = snaps.values.begin(); snap != snaps.values.end(); ++snap) {
        snap->screenIndex = idx;
        snap->snapIndex = 0;
    }

    int width = rect.width() / QnScreenSnap::snapsPerScreen();
    int height = rect.height() / QnScreenSnap::snapsPerScreen();

    for (int j = 0; j < QnScreenSnap::snapsPerScreen(); ++j) {
        for (int k = 0; k < QnScreenSnap::snapsPerScreen(); ++k) {
            QRect partRect(rect.x() + width * j, rect.y() + height * k, width, height);
            ModelScreenPart part(idx, j, k, partRect, q);
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

void QnVideowallManageWidgetPrivate::ModelScreen::paint(QPainter* painter, const TransformationProcess &process) const {
    {
        Q_Q(const QnVideowallManageWidget);
        QRect targetRect = Geometry::eroded(geometry, frameMargin);

        QPainterPath path;
        path.addRect(targetRect);

        QPen borderPen(q->colors().desktop);
        borderPen.setWidth(screenBorderWidth);

        QnScopedPainterPenRollback penRollback(painter, borderPen);
        QnScopedPainterBrushRollback brushRollback(painter, withAlpha(q->colors().desktop, screenBackgroundAlpha));

        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);

        painter->drawPath(path);
    }
    base_type::paint(painter, process);

}

void QnVideowallManageWidgetPrivate::ModelScreen::paintProposed(QPainter* painter, const QRect &proposedGeometry) const {
    if (free()) {
        QRect intersection = geometry.intersected(proposedGeometry);
        if (!intersection.isValid())
            return;

        if (intersection == geometry) {
            base_type::paintProposed(painter, proposedGeometry);
            return;
        }

        foreach (const ModelScreenPart &part, parts)
            if (part.geometry.intersects(intersection))
                paintDashBorder(painter, part.bodyPath());
    }
}

/************************************************************************/
/* ModelItem                                                            */
/************************************************************************/

QnVideowallManageWidgetPrivate::ModelItem::ModelItem(ItemType itemType, const QnUuid &id, QnVideowallManageWidget* q):
    BaseModelItem(QRect(), itemType, id, q)
{
    editable = true;
}

bool QnVideowallManageWidgetPrivate::ModelItem::free() const {
    return false;
}

void QnVideowallManageWidgetPrivate::ModelItem::setFree(bool value) {
    Q_UNUSED(value);
    NX_ASSERT(false, "Should never get here");
}

void QnVideowallManageWidgetPrivate::ModelItem::paintProposed(QPainter* painter, const QRect &proposedGeometry) const {
    base_type::paintProposed(painter, proposedGeometry);

    Q_Q(const QnVideowallManageWidget);
    QnScopedPainterPenRollback pen(painter, Qt::NoPen);
    QnScopedPainterBrushRollback brush(painter, q->colors().error);
    painter->drawPath(bodyPath());
}

QColor QnVideowallManageWidgetPrivate::ModelItem::baseColor() const {
    Q_Q(const QnVideowallManageWidget);
    return q->colors().item;
}

/************************************************************************/
/* TransformationProcess                                                */
/************************************************************************/
QnVideowallManageWidgetPrivate::TransformationProcess::TransformationProcess():
    value(ItemTransformation::None)
{
}


/************************************************************************/
/* QnVideowallManageWidgetPrivate                                       */
/************************************************************************/

QnVideowallManageWidgetPrivate::QnVideowallManageWidgetPrivate(QnVideowallManageWidget* q):
    QObject(),
    q_ptr(q)
{
    int screenNumber = 0;
    for (const auto rect: screenGeometries())
    {
        m_unitedGeometry = m_unitedGeometry.united(rect);
        m_screens.append({screenNumber++, rect, q});
    }
}

// TODO: #GDM #VW create iterators
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
}

void QnVideowallManageWidgetPrivate::loadFromResource(const QnVideoWallResourcePtr &videowall)
{
    QList<QRect> screens = screenGeometries();
    QnUuid pcUuid = qnSettings->pcUuid();

    foreach (const QnVideoWallItem &item, videowall->items()->getItems()) {
        if (item.pcUuid != pcUuid)
            continue;

        ModelItem modelItem(ItemType::Existing, item.uuid, q_ptr);
        modelItem.name = item.name;
        modelItem.snaps = item.screenSnaps;
        modelItem.geometry = item.screenSnaps.geometry(screens);
        m_items << modelItem;
        setFree(modelItem.snaps, false);
    }
}

void QnVideowallManageWidgetPrivate::submitToResource(const QnVideoWallResourcePtr &videowall) {

    QnUuid pcUuid = qnSettings->pcUuid();

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
    videowall->pcs()->addOrUpdateItem(pcData);

    /* Form list of items currently attached to this PC. */
    QSet<QnUuid> existingIds;
    foreach (const QnVideoWallItem &item, videowall->items()->getItems())
        if (item.pcUuid == pcUuid)
            existingIds << item.uuid;

    /* Remove from the list items that will be updated. */
    foreach (const ModelItem &modelItem, m_items) {
        existingIds.remove(modelItem.id);

        QnVideoWallItem item;
        if (modelItem.itemType == ItemType::Added || !videowall->items()->hasItem(modelItem.id)) {
            item.name = nx::utils::generateUniqueString([&videowall] () {
                QStringList used;
                foreach (const QnVideoWallItem &item, videowall->items()->getItems())
                    used << item.name;
                return used;
            }(), tr("Screen"), tr("Screen %1") );
            item.pcUuid = pcUuid;
            item.uuid = modelItem.id;
        } else {
            item = videowall->items()->getItem(modelItem.id);
        }
        item.screenSnaps = modelItem.snaps;
        videowall->items()->addOrUpdateItem(item);
    }

    /* Delete other items. */
    foreach (const QnUuid &toDelete, existingIds)
        videowall->items()->removeItem(toDelete);

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
    if (m_unitedGeometry.isNull())    // TODO: #GDM #VW replace by model.Valid
        return QRect();

    return Geometry::expanded(Geometry::aspectRatio(m_unitedGeometry), rect, Qt::KeepAspectRatio).toRect();
}

void QnVideowallManageWidgetPrivate::setFree(const QnScreenSnaps &snaps, bool value)
{
    QSet<int> screenIdxs = snaps.screens();
    // if the item takes some screens, it should take them fully
    if (screenIdxs.size() > 1) {
        for (int idx: screenIdxs) {
            if (idx >= m_screens.size())
                continue;   // we can lose one screen after snaps have been set
            NX_ASSERT(m_screens[idx].free() != value);
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
                NX_ASSERT(idx < screen.parts.size());
                NX_ASSERT(screen.parts[idx].free() != value);
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
        item.paint(painter, m_process);
    });
}

void QnVideowallManageWidgetPrivate::mouseEnter() {
    //do nothing for now
}

void QnVideowallManageWidgetPrivate::mouseLeave() {
    if (m_process.isRunning())
        return;

    foreachItem([this](BaseModelItem &item, bool &) {
        item.flags &= ~(StateFlag::Hovered | StateFlag::DeleteHovered);
    });
    QCursor cursor(transformationsCursor(ItemTransformation::None));
    q_ptr->setCursor(cursor);
}

void QnVideowallManageWidgetPrivate::mouseMoveAt(const QPoint &pos) {
    if (m_process.isRunning())
        return;

    ItemTransformations proposed = ItemTransformation::None;
    foreachItem([this, pos, &proposed](BaseModelItem &item, bool &) {
        if (item.geometry.contains(pos)) {
            item.flags |= StateFlag::Hovered;
            if (item.editable && m_process == ItemTransformation::None) {
                if (item.deleteButtonRect().contains(pos))
                    item.flags |= StateFlag::DeleteHovered;
                else
                    item.flags &= ~StateFlag::DeleteHovered;

                proposed = transformationsAnchor(item.bodyRect(), pos);
            }
        }
        else
            item.flags &= ~(StateFlag::Hovered | StateFlag::DeleteHovered);
    });

    QCursor cursor(transformationsCursor(proposed));
    q_ptr->setCursor(cursor);
}

void QnVideowallManageWidgetPrivate::mouseClickAt(const QPoint &pos, Qt::MouseButtons buttons) {
    if ((buttons & Qt::LeftButton) != Qt::LeftButton)
        return;
    if (m_process.isRunning())
        return;

    foreachItem([this, pos](BaseModelItem &item, bool &abort) {
        if (!item.geometry.contains(pos) || !item.free())
            return;

        ModelItem added(ItemType::Added, QnUuid::createUuid(), q_ptr);
        added.name = tr("New Item");
        added.geometry = item.geometry;
        added.snaps = item.snaps;
        added.flags |= StateFlag::Hovered;   //mouse cursor is over it
        added.opacity = 1.0;
        m_items << added;
        emit itemsChanged();

        item.setFree(false);

        abort = true;
    });

    for (auto item = m_items.begin(); item != m_items.end(); ++item) {
        if (!item->editable || !item->deleteButtonRect().contains(pos))
            continue;;

        if (item->itemType == ItemType::Existing)
        {
            QnMessageBox dialog(QnMessageBoxIcon::Question,
                tr("Delete \"%1\"?").arg(item->name), QString(),
                QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
                q_ptr);

            dialog.addCustomButton(QnMessageBoxCustomButton::Delete,
                QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
            if (dialog.exec() == QDialogButtonBox::Cancel)
                return;
        }

        setFree(item->snaps, true);
        m_items.removeAt(std::distance(m_items.begin(), item));
        emit itemsChanged();

        break;
    }
}

void QnVideowallManageWidgetPrivate::dragStartAt(const QPoint &pos) {
    NX_ASSERT(!m_process.isRunning());
    foreachItem([this, pos](BaseModelItem &item, bool &abort) {
        if (!item.editable || !item.geometry.contains(pos))
            return;
        abort = true;   //no more than one such item must exist

        m_process.value = transformationsAnchor(item.bodyRect(), pos);
        if (m_process == ItemTransformation::None)
            return;

        calculateProposedGeometryFunction func = [this](const BaseModelItem &item, bool *valid, bool *multiScreen) {
            if (m_process == ItemTransformation::Move)
                return calculateProposedMoveGeometry(item, valid, multiScreen);
            return calculateProposedResizeGeometry(item, valid, multiScreen);
        };
        processItemStart(item, func);
        m_process.geometry = item.geometry;
        m_process.oldGeometry = item.geometry;
    });
}

void QnVideowallManageWidgetPrivate::dragMoveAt(const QPoint &pos, const QPoint &oldPos) {
    if (!m_process.isRunning())
        return;

    foreachItem([this, &pos, &oldPos](BaseModelItem &item, bool &abort) {
        if (!item.hasFlag(StateFlag::Pressed))
            return;
        QPoint offset = pos - oldPos;
        if (m_process == ItemTransformation::Move)
            moveItem(item, offset);
        else
            resizeItem(item, offset);
        abort = true;
    });
}

void QnVideowallManageWidgetPrivate::dragEndAt(const QPoint &pos) {
    if (!m_process.isRunning())
        return;

    foreachItem([this, pos](BaseModelItem &item, bool &abort) {
        Q_UNUSED(abort)	//we should remove Hovered flag from all items
            item.flags &= ~StateFlags(StateFlag::Hovered);
        if (!item.hasFlag(StateFlag::Pressed))
            return;

        calculateProposedGeometryFunction func = [this](const BaseModelItem &item, bool *valid, bool *multiScreen) {
            if (m_process == ItemTransformation::Move)
                return calculateProposedMoveGeometry(item, valid, multiScreen);
            return calculateProposedResizeGeometry(item, valid, multiScreen);
        };
        processItemEnd(item, func);
    });

    m_process.clear();
}

void QnVideowallManageWidgetPrivate::tick(int deltaMSecs) {
    qreal opacityDelta = opacityChangeSpeed * deltaMSecs;

    foreachItem([this, opacityDelta](BaseModelItem &item, bool &) {
        if (item.hasFlag(StateFlag::Pressed)) {
            item.opacity = qMax(item.opacity - opacityDelta, draggedOpacity);
        } else {
            if (item.hasFlag(StateFlag::Hovered) && (m_process == ItemTransformation::None))
                item.opacity = qMin(item.opacity + opacityDelta, 1.0);
            else
                item.opacity = qMax(item.opacity - opacityDelta, minOpacity);
        }
        if (item.hasFlag(StateFlag::DeleteHovered)) {
            item.deleteButtonOpacity = qMin(item.deleteButtonOpacity + opacityDelta, 1.0);
        } else {
            item.deleteButtonOpacity = qMax(item.deleteButtonOpacity - opacityDelta, 0.0);
        }
    });
}

void QnVideowallManageWidgetPrivate::moveItem(BaseModelItem &item, const QPoint &offset) {
    item.geometry.translate(offset);
    m_process.geometry = calculateProposedMoveGeometry(item, NULL, NULL);
}

void QnVideowallManageWidgetPrivate::resizeItem(BaseModelItem &item, const QPoint &offset) {
    if (m_process.value & ItemTransformation::ResizeLeft)
        item.geometry.adjust(offset.x(), 0, 0, 0);
    else if (m_process.value & ItemTransformation::ResizeRight)
        item.geometry.adjust(0, 0, offset.x(), 0);

    if (m_process.value & ItemTransformation::ResizeTop)
        item.geometry.adjust(0, offset.y(), 0, 0);
    else if (m_process.value & ItemTransformation::ResizeBottom)
        item.geometry.adjust(0, 0, 0, offset.y());

    m_process.geometry = calculateProposedResizeGeometry(item, NULL, NULL);
}

QnVideowallManageWidgetPrivate::ItemTransformations QnVideowallManageWidgetPrivate::transformationsAnchor(const QRect &geometry, const QPoint &pos) const {
    QRect leftAnchor    (Geometry::dilated(QRect(geometry.left(), geometry.top(), 0, geometry.height()), transformationOffset));
    QRect centerXAnchor (Geometry::dilated(QRect(geometry.center().x(), geometry.top(), 0, geometry.height()), transformationOffset));
    QRect rightAnchor   (Geometry::dilated(QRect(geometry.left() + geometry.width(), geometry.top(), 0, geometry.height()), transformationOffset));

    QRect topAnchor     (Geometry::dilated(QRect(geometry.left(), geometry.top(), geometry.width(), 0), transformationOffset));
    QRect centerYAnchor (Geometry::dilated(QRect(geometry.left(), geometry.center().y(), geometry.width(), 0), transformationOffset));
    QRect bottomAnchor  (Geometry::dilated(QRect(geometry.left(), geometry.top() + geometry.height(), geometry.width(), 0), transformationOffset));

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

QRect QnVideowallManageWidgetPrivate::calculateProposedResizeGeometry(const BaseModelItem &item, bool *valid, bool *multiScreen) const {
    QRect geometry = item.geometry;
    QRect result;

    int screenCount = 0;
    for(auto screen = m_screens.cbegin(); screen != m_screens.cend(); ++screen) {
        bool intersectsPartOf = false;
        for(auto part = screen->parts.cbegin(); part != screen->parts.cend(); part++) {
            QRect intersected = part->geometry.intersected(geometry);
            if (1.0 * Geometry::area(intersected) / Geometry::area(part->geometry) > minAreaOverlapping) {
                if (valid)
                    *valid &= part->free();
                result = result.united(part->geometry);
                intersectsPartOf = true;
            }
        }
        if (intersectsPartOf)
            ++screenCount;
    }

    if (screenCount > 1) {
        for(auto screen = m_screens.cbegin(); screen != m_screens.cend(); ++screen) {
            if (result.intersects(screen->geometry)) {
                result = result.united(screen->geometry);
                if (valid)
                    *valid &= screen->free();
            }
        }
    }

    if (valid)
        *valid &= result.isValid();

    if (multiScreen)
        *multiScreen = screenCount > 1;
    return result;
}

template<typename T>
QList<T> bestMatchingGeometry(const QList<T> &source, const QRect &geometry, int count) {
    QList<T> result;
    result.reserve(count);
    for(int i = 0; i < count; ++i)
        result << source[0];

    std::partial_sort_copy(source.cbegin(), source.cend(), result.begin(), result.end(), [&geometry](const T& left, const T& right) {
        return Geometry::area(left.geometry.intersected(geometry)) > Geometry::area(right.geometry.intersected(geometry));
    });
    return result;
}

template<typename T>
QRect unitedGeometry(const QList<T> source, bool *valid) {
    QRect result;
    for(const T &item: source) {
        result = result.united(item.geometry);
        if (valid)
            *valid &= item.free();
    }
    return result;
}


QRect QnVideowallManageWidgetPrivate::calculateProposedMoveGeometry(const BaseModelItem &item, bool *valid, bool *multiScreen) const {
    QRect geometry = item.geometry;

    const int screenCount = item.snaps.screens().size();
    if (multiScreen)
        *multiScreen = screenCount > 1;

    QList<ModelScreen> bestScreens = bestMatchingGeometry(m_screens, geometry, screenCount);
    if (screenCount > 1 || !item.isPartOfScreen())
        return unitedGeometry(bestScreens, valid);

    int screenIdx = bestScreens[0].snaps.left().screenIndex;
    int partCount = [](const QnScreenSnaps &snaps) {
        int x = QnScreenSnap::snapsPerScreen() - snaps.right().snapIndex - snaps.left().snapIndex;
        int y = QnScreenSnap::snapsPerScreen() - snaps.bottom().snapIndex - snaps.top().snapIndex;
        return x * y;
    }(item.snaps);

    QList<ModelScreenPart> bestParts = bestMatchingGeometry(m_screens[screenIdx].parts, geometry, partCount);
    return unitedGeometry(bestParts, valid);
}

void QnVideowallManageWidgetPrivate::processItemStart(BaseModelItem &item, calculateProposedGeometryFunction proposedGeometry) {
    item.flags |= StateFlag::Pressed;
    setFree(item.snaps, true);
    m_process.geometry = proposedGeometry(item, NULL, NULL);
}

template<typename T>
QnScreenSnaps snapsFromGeometry(const QList<T> items, const QRect &geometry) {
    QRect united;
    QnScreenSnaps result;
    foreach(const T &item, items) {
        if (!item.geometry.intersects(geometry))
            continue;

        if (!united.isValid()) {
            united = item.geometry;
            result = item.snaps;
            continue;
        }

        if (item.geometry.left() < united.left())
            result.left() = item.snaps.left();
        if (item.geometry.right() > united.right())
            result.right() = item.snaps.right();
        if (item.geometry.top() < united.top())
            result.top() = item.snaps.top();
        if (item.geometry.bottom() > united.bottom())
            result.bottom() = item.snaps.bottom();
        united = united.united(item.geometry);
    }
    return result;
}

void QnVideowallManageWidgetPrivate::processItemEnd(BaseModelItem &item, calculateProposedGeometryFunction proposedGeometry) {
    item.flags &= ~StateFlags(StateFlag::Pressed);

    bool valid = true;
    bool multiscreen = false;
    QRect newGeometry = proposedGeometry(item, &valid, &multiscreen);

    if (!valid) {
        item.geometry = m_process.oldGeometry;
    } else {
        item.geometry = newGeometry;

        if (multiscreen) {
            item.snaps = snapsFromGeometry(m_screens, newGeometry);
        } else {
            foreach(const ModelScreen &screen, m_screens) {
                if (!screen.geometry.intersects(newGeometry))
                    continue;
                if (screen.geometry == newGeometry)
                    item.snaps = screen.snaps;
                else
                    item.snaps = snapsFromGeometry(screen.parts, newGeometry);
                break;
            }
        }
    }

    //for bes user experience do not hide controls until user moves a mouse
    setFree(item.snaps, false);
    item.flags |= StateFlags(StateFlag::Hovered);
}

int QnVideowallManageWidgetPrivate::proposedItemsCount() const {
    return m_items.size();
}
